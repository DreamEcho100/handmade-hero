#ifndef UTILS_BASE_H
#define UTILS_BASE_H

/* ── Canvas dimensions ───────────────────────────────────────────────────── */
#define GAME_W 512
#define GAME_H 480

/* ── Game-world unit system ──────────────────────────────────────────────── */
/* 1 game unit = 32 pixels at the reference resolution (512 × 480).
 * GAME_UNITS_W × GAME_UNITS_H = 16 × 15 game units cover the whole canvas. */
#define GAME_UNITS_W 16.0f
#define GAME_UNITS_H 15.0f

/* HUD_TOP_Y — convert an offset-from-top to a COORD_ORIGIN_LEFT_BOTTOM world-y.
 * With y-up coords, y=0 is screen bottom and y=GAME_UNITS_H is screen top.
 * HUD_TOP_Y(0.25) = 14.75  → places HUD 0.25 units below the top edge.
 * Use for any 2D overlay element anchored to the screen top (score, FPS). */
#define HUD_TOP_Y(offset_from_top)                                             \
  ((float)GAME_UNITS_H - (float)(offset_from_top))

/* ── Scaling Mode ────────────────────────────────────────────────────────── */
typedef enum {
  WINDOW_SCALE_MODE_FIXED,          /* Fixed 512×480 + GPU letterbox scaling */
  WINDOW_SCALE_MODE_DYNAMIC_MATCH,  /* Backbuffer matches window (may distort)*/
  WINDOW_SCALE_MODE_DYNAMIC_ASPECT, /* Backbuffer resizes, preserves 32:30    */
  WINDOW_SCALE_MODE_COUNT
} WindowScaleMode;

/* ── Coordinate origin ───────────────────────────────────────────────────────
 *
 * Controls where world-space (0,0) maps to in the window and which direction
 * each axis grows.  Passed to make_render_context(); baked into RenderContext
 * floats so the hot path (draw calls) has no branches.
 *
 * NAMING CONVENTION — horizontal qualifier first:
 *   "LEFT_BOTTOM"  = origin at the bottom-left corner.
 *   "LEFT_TOP"     = origin at the top-left corner.
 * The first word names the x-axis home side (LEFT/RIGHT);
 * the second word names the y-axis home side (BOTTOM/TOP).
 *
 * ZII: COORD_ORIGIN_LEFT_BOTTOM = 0 — uninitialized GameWorldConfig structs
 * automatically get the Handmade Hero / Cartesian default (y-up, x-right).
 *
 * | Origin              | x grows | y grows | (0,0) at      | Use case |
 * |---------------------|---------|---------|---------------|-------------------|
 * | LEFT_BOTTOM (def.)  | right   | up      | bottom-left   | HH, physics, this
 * | | LEFT_TOP            | right   | down    | top-left      | UI, web, screen
 * | | RIGHT_BOTTOM        | left    | up      | bottom-right  | RTL + Cartesian
 * | | RIGHT_TOP           | left    | down    | top-right     | RTL + screen |
 * | CENTER              | right   | up      | screen center | twin-stick games
 * | | CUSTOM              | either  | either  | anywhere      | sub-region,
 * misc  |
 */
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM =
      0,                     /* DEFAULT (ZII). x-right, y-up. HH/Cartesian. */
  COORD_ORIGIN_LEFT_TOP,     /* x-right, y-down. Screen/UI convention.       */
  COORD_ORIGIN_RIGHT_BOTTOM, /* x-left, y-up.  RTL + Cartesian.            */
  COORD_ORIGIN_RIGHT_TOP,    /* x-left, y-down.  RTL + screen convention.    */
  COORD_ORIGIN_CENTER,       /* x-up from center, y-right from center.       */
  COORD_ORIGIN_CUSTOM,       /* Game fills custom_* fields; dev owns axes.   */
} CoordOrigin;

/* Configuration struct passed by the game to make_render_context().
 * ZII default (all zeros) → COORD_ORIGIN_LEFT_BOTTOM, custom_* unused.
 *
 * camera_zoom must be set to 1.0f by the platform; 0.0 produces a degenerate
 * context where all coords map to 0 (everything invisible). */
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  /* Only used when coord_origin == COORD_ORIGIN_CUSTOM: */
  float custom_x_offset; /* world x that maps to pixel x=0                   */
  float custom_y_offset; /* world y that maps to pixel y=0                   */
  float custom_x_sign;   /* +1 = x grows right (LTR), -1 = left (RTL)       */
  float custom_y_sign;   /* +1 = y grows down,  -1 = y grows up              */
  /* Camera (pan + zoom).  Set explicitly; no ZII fallback for camera_zoom.
   * camera_x: world x of the left edge of the visible viewport.
   * camera_y: world y of the bottom edge (y-up) or top edge (y-down).
   * camera_zoom: 1.0 = no zoom; 2.0 = zoom in 2×; 0.5 = zoom out 2×. */
  float camera_x;
  float camera_y;
  float camera_zoom;
} GameWorldConfig;

/* ── Origin-aware axis sign helpers ─────────────────────────────────────────
 *
 * Return the axis direction (+1 or -1) for a given config.  Use when movement
 * or camera-pan direction must be correct for all origins.
 *
 * For COORD_ORIGIN_CUSTOM the helper forwards custom_x/y_sign directly.     */
static inline float world_config_y_sign(const GameWorldConfig *cfg) {
  if (cfg->coord_origin == COORD_ORIGIN_CUSTOM)
    return cfg->custom_y_sign;
  return (cfg->coord_origin == COORD_ORIGIN_LEFT_TOP ||
          cfg->coord_origin == COORD_ORIGIN_RIGHT_TOP)
             ? 1.0f
             : -1.0f;
}

static inline float world_config_x_sign(const GameWorldConfig *cfg) {
  if (cfg->coord_origin == COORD_ORIGIN_CUSTOM)
    return cfg->custom_x_sign;
  return (cfg->coord_origin == COORD_ORIGIN_RIGHT_BOTTOM ||
          cfg->coord_origin == COORD_ORIGIN_RIGHT_TOP)
             ? -1.0f
             : 1.0f;
}

#endif /* UTILS_BASE_H */
