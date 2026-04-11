#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H

#include "backbuffer.h"
#include "draw-text.h"
#include "math.h"

/* ══════════════════════════════════════════════════════════════════════
 *  REFERENCE RESOLUTION
 *  Design your game at this resolution. All "reference pixels" use this.
 * ══════════════════════════════════════════════════════════════════════ */
#define REF_W 800.0f
#define REF_H 600.0f
#define REF_ASPECT (REF_W / REF_H) /* 1.333... (4:3) */

/* ══════════════════════════════════════════════════════════════════════
 *  WORLD SPACE
 *  Game logic coordinates. Physics, AI, gameplay use these units.
 * ══════════════════════════════════════════════════════════════════════ */
#define WORLD_W 16.0f
#define WORLD_H 12.0f
#define WORLD_ASPECT (WORLD_W / WORLD_H) /* 1.333... (4:3) */

/* ══════════════════════════════════════════════════════════════════════
 *  ANCHOR PRESETS
 *  Common screen positions for UI elements.
 * ══════════════════════════════════════════════════════════════════════ */
typedef enum {
  ANCHOR_TOP_LEFT,
  ANCHOR_TOP_CENTER,
  ANCHOR_TOP_RIGHT,
  ANCHOR_CENTER_LEFT,
  ANCHOR_CENTER,
  ANCHOR_CENTER_RIGHT,
  ANCHOR_BOTTOM_LEFT,
  ANCHOR_BOTTOM_CENTER,
  ANCHOR_BOTTOM_RIGHT
} Anchor;

/* ══════════════════════════════════════════════════════════════════════
 *  SCALE MODE
 *  How to handle aspect ratio mismatches.
 * ══════════════════════════════════════════════════════════════════════ */
typedef enum {
  SCALE_FIT,     /* Fit inside, preserve aspect (letterbox) */
  SCALE_FILL,    /* Fill completely, preserve aspect (crop) */
  SCALE_STRETCH, /* Stretch to fill (distort) */
  SCALE_NONE     /* No scaling (1:1 pixels) */
} ScaleMode;

/* ══════════════════════════════════════════════════════════════════════
 *  RECT STRUCTURE
 *  Used for layout calculations, safe areas, viewports.
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
  float x, y, w, h;
} Rect;

static inline Rect rect_make(float x, float y, float w, float h) {
  return (Rect){x, y, w, h};
}

static inline float rect_right(Rect r) { return r.x + r.w; }
static inline float rect_bottom(Rect r) { return r.y + r.h; }
static inline float rect_center_x(Rect r) { return r.x + r.w * 0.5f; }
static inline float rect_center_y(Rect r) { return r.y + r.h * 0.5f; }

static inline Rect rect_inset(Rect r, float inset) {
  return rect_make(r.x + inset, r.y + inset, r.w - inset * 2.0f,
                   r.h - inset * 2.0f);
}

static inline Rect rect_inset_xy(Rect r, float inset_x, float inset_y) {
  return rect_make(r.x + inset_x, r.y + inset_y, r.w - inset_x * 2.0f,
                   r.h - inset_y * 2.0f);
}

/* ══════════════════════════════════════════════════════════════════════
 *  VEC2 STRUCTURE
 *  Simple 2D vector for positions, sizes, etc.
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
  float x, y;
} Vec2;

static inline Vec2 vec2_make(float x, float y) { return (Vec2){x, y}; }

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
  return vec2_make(a.x + b.x, a.y + b.y);
}

static inline Vec2 vec2_scale(Vec2 v, float s) {
  return vec2_make(v.x * s, v.y * s);
}

/* ══════════════════════════════════════════════════════════════════════
 *  RENDER CONTEXT
 *  Central structure for all coordinate conversions.
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
  Backbuffer *backbuffer;

  /* Actual screen size */
  float screen_w;
  float screen_h;
  float screen_aspect;

  /* Scale factors (screen / reference) */
  float scale_x;
  float scale_y;
  float scale_uniform; /* MIN(scale_x, scale_y) for aspect-correct scaling */

  /* World to reference conversion */
  float world_to_ref; /* REF_W / WORLD_W = 50.0 */

  /* Safe area (accounts for notches, etc.) */
  Rect safe_area;

  /* Viewport (visible game area after letterboxing) */
  Rect viewport;

  /* Text scaling */
  float text_scale;
  float line_height;
  float line_height_large;

  /* Camera (for world space) */
  Vec2 camera_pos;
  float camera_zoom;

} RenderContext;

/* ══════════════════════════════════════════════════════════════════════
 *  CONTEXT CREATION
 * ══════════════════════════════════════════════════════════════════════ */
static inline RenderContext make_render_context(Backbuffer *bb) {
  RenderContext ctx = {0};
  ctx.backbuffer = bb;

  /* Screen dimensions */
  ctx.screen_w = (float)bb->width;
  ctx.screen_h = (float)bb->height;
  ctx.screen_aspect = ctx.screen_w / ctx.screen_h;

  /* Scale factors */
  ctx.scale_x = ctx.screen_w / REF_W;
  ctx.scale_y = ctx.screen_h / REF_H;
  ctx.scale_uniform = MIN(ctx.scale_x, ctx.scale_y);

  /* World conversion */
  ctx.world_to_ref = REF_W / WORLD_W; /* 50.0 */

  /* Safe area (full screen by default, can be adjusted for notches) */
  ctx.safe_area = rect_make(0, 0, ctx.screen_w, ctx.screen_h);

  /* Viewport (letterboxed area) */
  float vp_w = REF_W * ctx.scale_uniform;
  float vp_h = REF_H * ctx.scale_uniform;
  float vp_x = (ctx.screen_w - vp_w) * 0.5f;
  float vp_y = (ctx.screen_h - vp_h) * 0.5f;
  ctx.viewport = rect_make(vp_x, vp_y, vp_w, vp_h);

  /* Text scaling */
  ctx.text_scale = MAX(ctx.scale_uniform, 1.0f);
  ctx.line_height = GLYPH_H * ctx.text_scale * 1.4f;
  ctx.line_height_large = GLYPH_H * ctx.text_scale * 2.0f * 1.4f;

  /* Camera defaults */
  ctx.camera_pos = vec2_make(WORLD_W * 0.5f, WORLD_H * 0.5f);
  ctx.camera_zoom = 1.0f;

  return ctx;
}

/* ══════════════════════════════════════════════════════════════════════
 *  COORDINATE CONVERSIONS: WORLD SPACE
 *  Game units (16×12) → Screen pixels
 * ══════════════════════════════════════════════════════════════════════ */

/* World position → screen pixels (no camera) */
static inline float world_to_screen_x(RenderContext *ctx, float world_x) {
  float ref_x = world_x * ctx->world_to_ref;
  return ctx->viewport.x + ref_x * ctx->scale_uniform;
}

static inline float world_to_screen_y(RenderContext *ctx, float world_y) {
  float ref_y = world_y * ctx->world_to_ref;
  return ctx->viewport.y + ref_y * ctx->scale_uniform;
}

static inline Vec2 world_to_screen(RenderContext *ctx, Vec2 world_pos) {
  return vec2_make(world_to_screen_x(ctx, world_pos.x),
                   world_to_screen_y(ctx, world_pos.y));
}

/* World size → screen pixels */
static inline float world_to_screen_w(RenderContext *ctx, float world_w) {
  return world_w * ctx->world_to_ref * ctx->scale_uniform;
}

static inline float world_to_screen_h(RenderContext *ctx, float world_h) {
  return world_h * ctx->world_to_ref * ctx->scale_uniform;
}

static inline Vec2 world_to_screen_size(RenderContext *ctx, Vec2 world_size) {
  return vec2_make(world_to_screen_w(ctx, world_size.x),
                   world_to_screen_h(ctx, world_size.y));
}

/* World position → screen pixels (WITH camera) */
static inline float world_to_screen_cam_x(RenderContext *ctx, float world_x) {
  float cam_offset_x = world_x - ctx->camera_pos.x;
  float zoomed_x = cam_offset_x * ctx->camera_zoom;
  float centered_x = zoomed_x + WORLD_W * 0.5f;
  return world_to_screen_x(ctx, centered_x);
}

static inline float world_to_screen_cam_y(RenderContext *ctx, float world_y) {
  float cam_offset_y = world_y - ctx->camera_pos.y;
  float zoomed_y = cam_offset_y * ctx->camera_zoom;
  float centered_y = zoomed_y + WORLD_H * 0.5f;
  return world_to_screen_y(ctx, centered_y);
}

static inline Vec2 world_to_screen_cam(RenderContext *ctx, Vec2 world_pos) {
  return vec2_make(world_to_screen_cam_x(ctx, world_pos.x),
                   world_to_screen_cam_y(ctx, world_pos.y));
}

/* Screen pixels → world position (for mouse input) */
static inline float screen_to_world_x(RenderContext *ctx, float screen_x) {
  float vp_x = screen_x - ctx->viewport.x;
  float ref_x = vp_x / ctx->scale_uniform;
  return ref_x / ctx->world_to_ref;
}

static inline float screen_to_world_y(RenderContext *ctx, float screen_y) {
  float vp_y = screen_y - ctx->viewport.y;
  float ref_y = vp_y / ctx->scale_uniform;
  return ref_y / ctx->world_to_ref;
}

static inline Vec2 screen_to_world(RenderContext *ctx, Vec2 screen_pos) {
  return vec2_make(screen_to_world_x(ctx, screen_pos.x),
                   screen_to_world_y(ctx, screen_pos.y));
}

/* ══════════════════════════════════════════════════════════════════════
 *  COORDINATE CONVERSIONS: REFERENCE SPACE
 *  Reference pixels (800×600) → Screen pixels
 * ══════════════════════════════════════════════════════════════════════ */

/* Reference position → screen pixels */
static inline float ref_to_screen_x(RenderContext *ctx, float ref_x) {
  return ctx->viewport.x + ref_x * ctx->scale_uniform;
}

static inline float ref_to_screen_y(RenderContext *ctx, float ref_y) {
  return ctx->viewport.y + ref_y * ctx->scale_uniform;
}

static inline Vec2 ref_to_screen(RenderContext *ctx, Vec2 ref_pos) {
  return vec2_make(ref_to_screen_x(ctx, ref_pos.x),
                   ref_to_screen_y(ctx, ref_pos.y));
}

/* Reference size → screen pixels */
static inline float ref_to_screen_w(RenderContext *ctx, float ref_w) {
  return ref_w * ctx->scale_uniform;
}

static inline float ref_to_screen_h(RenderContext *ctx, float ref_h) {
  return ref_h * ctx->scale_uniform;
}

/* ══════════════════════════════════════════════════════════════════════
 *  COORDINATE CONVERSIONS: NORMALIZED SPACE
 *  Normalized (0-1) → Screen pixels
 * ══════════════════════════════════════════════════════════════════════ */

/* Normalized position → screen pixels (full screen) */
static inline float norm_to_screen_x(RenderContext *ctx, float norm_x) {
  return norm_x * ctx->screen_w;
}

static inline float norm_to_screen_y(RenderContext *ctx, float norm_y) {
  return norm_y * ctx->screen_h;
}

static inline Vec2 norm_to_screen(RenderContext *ctx, Vec2 norm_pos) {
  return vec2_make(norm_to_screen_x(ctx, norm_pos.x),
                   norm_to_screen_y(ctx, norm_pos.y));
}

/* Normalized position → screen pixels (within safe area) */
static inline float norm_to_safe_x(RenderContext *ctx, float norm_x) {
  return ctx->safe_area.x + norm_x * ctx->safe_area.w;
}

static inline float norm_to_safe_y(RenderContext *ctx, float norm_y) {
  return ctx->safe_area.y + norm_y * ctx->safe_area.h;
}

static inline Vec2 norm_to_safe(RenderContext *ctx, Vec2 norm_pos) {
  return vec2_make(norm_to_safe_x(ctx, norm_pos.x),
                   norm_to_safe_y(ctx, norm_pos.y));
}

/* ══════════════════════════════════════════════════════════════════════
 *  ANCHOR SYSTEM
 *  Position elements relative to screen edges/corners.
 * ══════════════════════════════════════════════════════════════════════ */

/* Get anchor position in screen pixels */
static inline Vec2 get_anchor_pos(RenderContext *ctx, Anchor anchor) {
  Rect sa = ctx->safe_area;

  float x, y;

  switch (anchor) {
  case ANCHOR_TOP_LEFT:
    x = sa.x;
    y = sa.y;
    break;
  case ANCHOR_TOP_CENTER:
    x = rect_center_x(sa);
    y = sa.y;
    break;
  case ANCHOR_TOP_RIGHT:
    x = rect_right(sa);
    y = sa.y;
    break;
  case ANCHOR_CENTER_LEFT:
    x = sa.x;
    y = rect_center_y(sa);
    break;
  case ANCHOR_CENTER:
    x = rect_center_x(sa);
    y = rect_center_y(sa);
    break;
  case ANCHOR_CENTER_RIGHT:
    x = rect_right(sa);
    y = rect_center_y(sa);
    break;
  case ANCHOR_BOTTOM_LEFT:
    x = sa.x;
    y = rect_bottom(sa);
    break;
  case ANCHOR_BOTTOM_CENTER:
    x = rect_center_x(sa);
    y = rect_bottom(sa);
    break;
  case ANCHOR_BOTTOM_RIGHT:
    x = rect_right(sa);
    y = rect_bottom(sa);
    break;
  default:
    x = sa.x;
    y = sa.y;
    break;
  }

  return vec2_make(x, y);
}

/* Position element at anchor with offset (in reference pixels) */
static inline Vec2 anchor_offset(RenderContext *ctx, Anchor anchor,
                                 float offset_x, float offset_y) {
  Vec2 anchor_pos = get_anchor_pos(ctx, anchor);
  float scaled_offset_x = offset_x * ctx->scale_uniform;
  float scaled_offset_y = offset_y * ctx->scale_uniform;
  return vec2_make(anchor_pos.x + scaled_offset_x,
                   anchor_pos.y + scaled_offset_y);
}

/* Position element at anchor with pivot (element's own anchor point) */
static inline Vec2 anchor_pivot(RenderContext *ctx, Anchor anchor,
                                float element_w, float element_h, float pivot_x,
                                float pivot_y, float margin_x, float margin_y) {
  Vec2 anchor_pos = get_anchor_pos(ctx, anchor);

  /* Pivot offset (0,0 = top-left, 0.5,0.5 = center, 1,1 = bottom-right) */
  float pivot_offset_x = -element_w * pivot_x;
  float pivot_offset_y = -element_h * pivot_y;

  /* Margin direction based on anchor */
  float margin_dir_x = 1.0f;
  float margin_dir_y = 1.0f;

  switch (anchor) {
  case ANCHOR_TOP_RIGHT:
  case ANCHOR_CENTER_RIGHT:
  case ANCHOR_BOTTOM_RIGHT:
    margin_dir_x = -1.0f;
    break;
  default:
    break;
  }

  switch (anchor) {
  case ANCHOR_BOTTOM_LEFT:
  case ANCHOR_BOTTOM_CENTER:
  case ANCHOR_BOTTOM_RIGHT:
    margin_dir_y = -1.0f;
    break;
  default:
    break;
  }

  float scaled_margin_x = margin_x * ctx->scale_uniform * margin_dir_x;
  float scaled_margin_y = margin_y * ctx->scale_uniform * margin_dir_y;

  return vec2_make(anchor_pos.x + pivot_offset_x + scaled_margin_x,
                   anchor_pos.y + pivot_offset_y + scaled_margin_y);
}

/* ══════════════════════════════════════════════════════════════════════
 *  ASPECT RATIO HELPERS
 *  Fit, fill, or stretch content to a target area.
 * ══════════════════════════════════════════════════════════════════════ */

/* Compute destination rect for content with given aspect ratio */
static inline Rect fit_aspect(Rect container, float content_aspect,
                              ScaleMode mode) {
  float container_aspect = container.w / container.h;

  float dst_w, dst_h;

  switch (mode) {
  case SCALE_FIT:
    /* Fit inside (letterbox) */
    if (container_aspect > content_aspect) {
      dst_h = container.h;
      dst_w = dst_h * content_aspect;
    } else {
      dst_w = container.w;
      dst_h = dst_w / content_aspect;
    }
    break;

  case SCALE_FILL:
    /* Fill completely (crop) */
    if (container_aspect > content_aspect) {
      dst_w = container.w;
      dst_h = dst_w / content_aspect;
    } else {
      dst_h = container.h;
      dst_w = dst_h * content_aspect;
    }
    break;

  case SCALE_STRETCH:
    /* Stretch to fill (distort) */
    dst_w = container.w;
    dst_h = container.h;
    break;

  case SCALE_NONE:
  default:
    /* No scaling */
    dst_w = container.w;
    dst_h = container.h;
    break;
  }

  /* Center in container */
  float dst_x = container.x + (container.w - dst_w) * 0.5f;
  float dst_y = container.y + (container.h - dst_h) * 0.5f;

  return rect_make(dst_x, dst_y, dst_w, dst_h);
}

/* ══════════════════════════════════════════════════════════════════════
 *  SAFE AREA
 *  Account for notches, rounded corners, system UI.
 * ══════════════════════════════════════════════════════════════════════ */

/* Set safe area insets (in screen pixels) */
static inline void set_safe_area_insets(RenderContext *ctx, float top,
                                        float right, float bottom, float left) {
  ctx->safe_area = rect_make(left, top, ctx->screen_w - left - right,
                             ctx->screen_h - top - bottom);
}

/* Set safe area as percentage of screen */
static inline void set_safe_area_percent(RenderContext *ctx, float percent) {
  float inset_x = ctx->screen_w * percent;
  float inset_y = ctx->screen_h * percent;
  ctx->safe_area = rect_inset_xy(rect_make(0, 0, ctx->screen_w, ctx->screen_h),
                                 inset_x, inset_y);
}

/* ══════════════════════════════════════════════════════════════════════
 *  CAMERA UTILITIES
 *  Pan, zoom, follow, shake.
 * ══════════════════════════════════════════════════════════════════════ */

/* Set camera position (world units) */
static inline void camera_set_pos(RenderContext *ctx, float x, float y) {
  ctx->camera_pos = vec2_make(x, y);
}

/* Move camera by delta (world units) */
static inline void camera_move(RenderContext *ctx, float dx, float dy) {
  ctx->camera_pos.x += dx;
  ctx->camera_pos.y += dy;
}

/* Set camera zoom (1.0 = normal, 2.0 = 2x zoom in) */
static inline void camera_set_zoom(RenderContext *ctx, float zoom) {
  ctx->camera_zoom = MAX(zoom, 0.1f); /* Prevent zero/negative zoom */
}

/* Smoothly follow a target (world units) */
static inline void camera_follow(RenderContext *ctx, float target_x,
                                 float target_y, float smoothing) {
  /* smoothing: 0.0 = instant, 1.0 = no movement */
  ctx->camera_pos.x += (target_x - ctx->camera_pos.x) * (1.0f - smoothing);
  ctx->camera_pos.y += (target_y - ctx->camera_pos.y) * (1.0f - smoothing);
}

/* Clamp camera to world bounds */
static inline void camera_clamp_to_world(RenderContext *ctx) {
  /* Visible area in world units */
  float visible_w = WORLD_W / ctx->camera_zoom;
  float visible_h = WORLD_H / ctx->camera_zoom;
  float half_w = visible_w * 0.5f;
  float half_h = visible_h * 0.5f;

  /* Clamp camera center */
  ctx->camera_pos.x = CLAMP(ctx->camera_pos.x, half_w, WORLD_W - half_w);
  ctx->camera_pos.y = CLAMP(ctx->camera_pos.y, half_h, WORLD_H - half_h);
}

/* ══════════════════════════════════════════════════════════════════════
 *  TEXT CURSOR
 *  Helper for flowing text layout.
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct {
  float x;         /* Current X in screen pixels */
  float y;         /* Current Y in screen pixels */
  float start_x;   /* Starting X (for newlines) */
  float scale;     /* Text scale */
  float line_h;    /* Line height in screen pixels */
  float max_width; /* Max width before wrap (0 = no wrap) */
} TextCursor;

static inline TextCursor make_text_cursor_at(RenderContext *ctx, float screen_x,
                                             float screen_y) {
  TextCursor cursor;
  cursor.x = screen_x;
  cursor.y = screen_y;
  cursor.start_x = screen_x;
  cursor.scale = ctx->text_scale;
  cursor.line_h = ctx->line_height;
  cursor.max_width = 0;
  return cursor;
}

static inline TextCursor make_text_cursor_world(RenderContext *ctx,
                                                float world_x, float world_y) {
  return make_text_cursor_at(ctx, world_to_screen_x(ctx, world_x),
                             world_to_screen_y(ctx, world_y));
}

static inline TextCursor make_text_cursor_ref(RenderContext *ctx, float ref_x,
                                              float ref_y) {
  return make_text_cursor_at(ctx, ref_to_screen_x(ctx, ref_x),
                             ref_to_screen_y(ctx, ref_y));
}

static inline TextCursor make_text_cursor_anchor(RenderContext *ctx,
                                                 Anchor anchor, float margin_x,
                                                 float margin_y) {
  Vec2 pos = anchor_offset(ctx, anchor, margin_x, margin_y);
  return make_text_cursor_at(ctx, pos.x, pos.y);
}

static inline void cursor_newline(TextCursor *cursor) {
  cursor->x = cursor->start_x;
  cursor->y += cursor->line_h;
}

static inline void cursor_advance(TextCursor *cursor, float amount) {
  cursor->x += amount;
}

static inline void cursor_gap(TextCursor *cursor, float multiplier) {
  cursor->y += cursor->line_h * multiplier;
}

static inline void cursor_set_scale(TextCursor *cursor, float scale,
                                    float line_height) {
  cursor->scale = scale;
  cursor->line_h = line_height;
}

/* Get text width in screen pixels */
static inline float text_width(const char *text, float scale) {
  float width = 0;
  for (const char *ch = text; *ch; ch++) {
    if (*ch != '\n') {
      width += GLYPH_W * scale;
    }
  }
  return width;
}

/* ══════════════════════════════════════════════════════════════════════
 *  LAYOUT HELPERS
 *  Grid, horizontal/vertical stacks.
 * ══════════════════════════════════════════════════════════════════════ */

/* Compute position in a grid layout */
static inline Vec2 grid_pos(Rect container, int cols, int rows, int col,
                            int row, float padding) {
  float cell_w = (container.w - padding * (cols + 1)) / cols;
  float cell_h = (container.h - padding * (rows + 1)) / rows;

  float x = container.x + padding + col * (cell_w + padding);
  float y = container.y + padding + row * (cell_h + padding);

  return vec2_make(x, y);
}

/* Compute size of a grid cell */
static inline Vec2 grid_cell_size(Rect container, int cols, int rows,
                                  float padding) {
  float cell_w = (container.w - padding * (cols + 1)) / cols;
  float cell_h = (container.h - padding * (rows + 1)) / rows;
  return vec2_make(cell_w, cell_h);
}

/* Horizontal stack layout */
typedef struct {
  float x;
  float y;
  float spacing;
  float height;
} HStack;

static inline HStack hstack_make(float x, float y, float spacing,
                                 float height) {
  return (HStack){x, y, spacing, height};
}

static inline Vec2 hstack_next(HStack *stack, float item_width) {
  Vec2 pos = vec2_make(stack->x, stack->y);
  stack->x += item_width + stack->spacing;
  return pos;
}

/* Vertical stack layout */
typedef struct {
  float x;
  float y;
  float spacing;
  float width;
} VStack;

static inline VStack vstack_make(float x, float y, float spacing, float width) {
  return (VStack){x, y, spacing, width};
}

static inline Vec2 vstack_next(VStack *stack, float item_height) {
  Vec2 pos = vec2_make(stack->x, stack->y);
  stack->y += item_height + stack->spacing;
  return pos;
}

/* ══════════════════════════════════════════════════════════════════════
 *  ANIMATION HELPERS
 *  Easing functions and interpolation.
 * ══════════════════════════════════════════════════════════════════════ */

/* Linear interpolation */
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
  return vec2_make(lerp(a.x, b.x, t), lerp(a.y, b.y, t));
}

/* Easing functions (t goes from 0 to 1) */
static inline float ease_in_quad(float t) { return t * t; }

static inline float ease_out_quad(float t) { return t * (2.0f - t); }

static inline float ease_in_out_quad(float t) {
  return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

static inline float ease_in_cubic(float t) { return t * t * t; }

static inline float ease_out_cubic(float t) {
  float t1 = t - 1.0f;
  return t1 * t1 * t1 + 1.0f;
}

static inline float ease_in_out_cubic(float t) {
  return t < 0.5f ? 4.0f * t * t * t
                  : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

/* Elastic easing (bouncy) */
static inline float ease_out_elastic(float t) {
  if (t == 0.0f || t == 1.0f)
    return t;
  float p = 0.3f;
  float s = p / 4.0f;
  return powf(2.0f, -10.0f * t) * sinf((t - s) * (2.0f * 3.14159f) / p) + 1.0f;
}

/* Bounce easing */
static inline float ease_out_bounce(float t) {
  if (t < 1.0f / 2.75f) {
    return 7.5625f * t * t;
  } else if (t < 2.0f / 2.75f) {
    t -= 1.5f / 2.75f;
    return 7.5625f * t * t + 0.75f;
  } else if (t < 2.5f / 2.75f) {
    t -= 2.25f / 2.75f;
    return 7.5625f * t * t + 0.9375f;
  } else {
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
  }
}

/* ══════════════════════════════════════════════════════════════════════
 *  SCREEN SHAKE
 *  Camera shake effect for impacts, explosions, etc.
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct {
  float intensity; /* Current shake intensity (world units) */
  float duration;  /* Total duration */
  float elapsed;   /* Time elapsed */
  float frequency; /* Shake frequency (higher = faster) */
  Vec2 offset;     /* Current offset to apply */
} ScreenShake;

static inline ScreenShake shake_make(float intensity, float duration,
                                     float frequency) {
  ScreenShake shake;
  shake.intensity = intensity;
  shake.duration = duration;
  shake.elapsed = 0.0f;
  shake.frequency = frequency;
  shake.offset = vec2_make(0, 0);
  return shake;
}

/* Simple pseudo-random for shake (deterministic, no state) */
static inline float shake_random(float seed) {
  /* Simple hash-based pseudo-random */
  int i = (int)(seed * 12345.6789f);
  i = (i << 13) ^ i;
  return 1.0f - ((i * (i * i * 15731 + 789221) + 1376312589) & 0x7fffffff) /
                    1073741824.0f;
}

static inline void shake_update(ScreenShake *shake, float dt) {
  if (shake->elapsed >= shake->duration) {
    shake->offset = vec2_make(0, 0);
    return;
  }

  shake->elapsed += dt;

  /* Decay intensity over time */
  float progress = shake->elapsed / shake->duration;
  float current_intensity = shake->intensity * (1.0f - progress);

  /* Generate offset using time-based noise */
  float t = shake->elapsed * shake->frequency;
  shake->offset.x = shake_random(t) * current_intensity;
  shake->offset.y = shake_random(t + 100.0f) * current_intensity;
}

static inline int shake_is_active(ScreenShake *shake) {
  return shake->elapsed < shake->duration;
}

/* Apply shake to camera */
static inline void camera_apply_shake(RenderContext *ctx, ScreenShake *shake) {
  ctx->camera_pos.x += shake->offset.x;
  ctx->camera_pos.y += shake->offset.y;
}

/* ══════════════════════════════════════════════════════════════════════
 *  VISIBILITY / CULLING
 *  Check if objects are on screen before drawing.
 * ══════════════════════════════════════════════════════════════════════ */

/* Check if a world-space rect is visible on screen */
static inline int is_visible_world(RenderContext *ctx, float world_x,
                                   float world_y, float world_w,
                                   float world_h) {
  /* Convert to screen space */
  float sx = world_to_screen_x(ctx, world_x);
  float sy = world_to_screen_y(ctx, world_y);
  float sw = world_to_screen_w(ctx, world_w);
  float sh = world_to_screen_h(ctx, world_h);

  /* Check against viewport */
  return !(sx + sw < ctx->viewport.x || sy + sh < ctx->viewport.y ||
           sx > rect_right(ctx->viewport) || sy > rect_bottom(ctx->viewport));
}

/* Check if a world-space point is visible */
static inline int is_point_visible_world(RenderContext *ctx, float world_x,
                                         float world_y) {
  float sx = world_to_screen_x(ctx, world_x);
  float sy = world_to_screen_y(ctx, world_y);

  return sx >= ctx->viewport.x && sx <= rect_right(ctx->viewport) &&
         sy >= ctx->viewport.y && sy <= rect_bottom(ctx->viewport);
}

/* Get visible world bounds (for culling) */
static inline Rect get_visible_world_bounds(RenderContext *ctx) {
  float min_x = screen_to_world_x(ctx, ctx->viewport.x);
  float min_y = screen_to_world_y(ctx, ctx->viewport.y);
  float max_x = screen_to_world_x(ctx, rect_right(ctx->viewport));
  float max_y = screen_to_world_y(ctx, rect_bottom(ctx->viewport));

  return rect_make(min_x, min_y, max_x - min_x, max_y - min_y);
}

/* ══════════════════════════════════════════════════════════════════════
 *  DEBUG DRAWING
 *  Helpers for visualizing coordinate systems, bounds, etc.
 * ══════════════════════════════════════════════════════════════════════ */

#ifdef DEBUG

/* Draw world grid (useful for debugging) */
static inline void debug_draw_world_grid(RenderContext *ctx, Backbuffer *bb,
                                         float r, float g, float b, float a) {
  /* Vertical lines */
  for (int x = 0; x <= (int)WORLD_W; x++) {
    float sx = world_to_screen_x(ctx, (float)x);
    float sy0 = world_to_screen_y(ctx, 0);
    float sy1 = world_to_screen_y(ctx, WORLD_H);

    /* Draw thin vertical line */
    draw_rect(bb, sx, sy0, 1.0f, sy1 - sy0, r, g, b, a);
  }

  /* Horizontal lines */
  for (int y = 0; y <= (int)WORLD_H; y++) {
    float sy = world_to_screen_y(ctx, (float)y);
    float sx0 = world_to_screen_x(ctx, 0);
    float sx1 = world_to_screen_x(ctx, WORLD_W);

    /* Draw thin horizontal line */
    draw_rect(bb, sx0, sy, sx1 - sx0, 1.0f, r, g, b, a);
  }
}

/* Draw safe area outline */
static inline void debug_draw_safe_area(RenderContext *ctx, Backbuffer *bb,
                                        float r, float g, float b, float a) {
  Rect sa = ctx->safe_area;
  float thickness = 2.0f;

  /* Top */
  draw_rect(bb, sa.x, sa.y, sa.w, thickness, r, g, b, a);
  /* Bottom */
  draw_rect(bb, sa.x, rect_bottom(sa) - thickness, sa.w, thickness, r, g, b, a);
  /* Left */
  draw_rect(bb, sa.x, sa.y, thickness, sa.h, r, g, b, a);
  /* Right */
  draw_rect(bb, rect_right(sa) - thickness, sa.y, thickness, sa.h, r, g, b, a);
}

/* Draw viewport outline */
static inline void debug_draw_viewport(RenderContext *ctx, Backbuffer *bb,
                                       float r, float g, float b, float a) {
  Rect vp = ctx->viewport;
  float thickness = 2.0f;

  /* Top */
  draw_rect(bb, vp.x, vp.y, vp.w, thickness, r, g, b, a);
  /* Bottom */
  draw_rect(bb, vp.x, rect_bottom(vp) - thickness, vp.w, thickness, r, g, b, a);
  /* Left */
  draw_rect(bb, vp.x, vp.y, thickness, vp.h, r, g, b, a);
  /* Right */
  draw_rect(bb, rect_right(vp) - thickness, vp.y, thickness, vp.h, r, g, b, a);
}

/* Draw anchor points */
static inline void debug_draw_anchors(RenderContext *ctx, Backbuffer *bb,
                                      float r, float g, float b, float a) {
  float size = 10.0f;

  for (int i = 0; i <= ANCHOR_BOTTOM_RIGHT; i++) {
    Vec2 pos = get_anchor_pos(ctx, (Anchor)i);
    draw_rect(bb, pos.x - size * 0.5f, pos.y - size * 0.5f, size, size, r, g, b,
              a);
  }
}

#endif /* DEBUG */

/* ══════════════════════════════════════════════════════════════════════
 *  CONVENIENCE MACROS
 *  Shorter names for common operations.
 * ══════════════════════════════════════════════════════════════════════ */

/* World space shortcuts */
#define W2S_X(ctx, x) world_to_screen_x(ctx, x)
#define W2S_Y(ctx, y) world_to_screen_y(ctx, y)
#define W2S_W(ctx, w) world_to_screen_w(ctx, w)
#define W2S_H(ctx, h) world_to_screen_h(ctx, h)

/* Reference space shortcuts */
#define R2S_X(ctx, x) ref_to_screen_x(ctx, x)
#define R2S_Y(ctx, y) ref_to_screen_y(ctx, y)
#define R2S_W(ctx, w) ref_to_screen_w(ctx, w)
#define R2S_H(ctx, h) ref_to_screen_h(ctx, h)

/* Normalized space shortcuts */
#define N2S_X(ctx, x) norm_to_screen_x(ctx, x)
#define N2S_Y(ctx, y) norm_to_screen_y(ctx, y)

#endif /* RENDER_CONTEXT_H */
