#ifndef UTILS_BASE_H
#define UTILS_BASE_H

/* ── Canvas dimensions ────────────────────────────────────────────────── */
#define GAME_W 800
#define GAME_H 600

/* ── Game-world unit system ───────────────────────────────────────────── */
#define GAME_UNITS_W 16.0f
#define GAME_UNITS_H 12.0f

/* HUD_TOP_Y — convert an offset-from-top to a BOTTOM_LEFT world-y coordinate.
 * With COORD_ORIGIN_LEFT_BOTTOM, y=0 is screen bottom and y=GAME_UNITS_H is
 * screen top.  HUD_TOP_Y(0.08) = 11.92 → places HUD 0.08 units from the top.
 * Use for any 2D overlay element that should be anchored to the screen top. */
#define HUD_TOP_Y(offset_from_top)                                             \
  ((float)GAME_UNITS_H - (float)(offset_from_top))

/* ── Scaling Mode ─────────────────────────────────────────────────────── */
typedef enum {
  WINDOW_SCALE_MODE_FIXED,         /* Fixed 800×600 + GPU letterbox scaling */
  WINDOW_SCALE_MODE_DYNAMIC_MATCH, /* Backbuffer matches window (can distort) */
  WINDOW_SCALE_MODE_DYNAMIC_ASPECT, /* Backbuffer resizes but preserves 4:3 */
  WINDOW_SCALE_MODE_COUNT
} WindowScaleMode;

/* ── Coordinate origin ────────────────────────────────────────────────────
 *
 * Selects where world-space (0,0) maps to in the window, and which direction
 * each axis grows.  Passed to make_render_context() once per frame; baked
 * into RenderContext floats so the hot path (draw calls) has no branches.
 *
 * NAMING CONVENTION — horizontal qualifier first:
 *   "LEFT_BOTTOM"  = origin is at the bottom-left corner.
 *   "RIGHT_TOP"    = origin is at the top-right corner.
 *
 * ZII: COORD_ORIGIN_LEFT_BOTTOM = 0 — uninitialized GameWorldConfig structs
 * automatically get the Handmade Hero / Cartesian default.
 *
 * ASTEROIDS default: COORD_ORIGIN_LEFT_BOTTOM (y-up Cartesian).
 *   (0,0) = bottom-left corner.  y increases upward.
 */
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM = 0, /* DEFAULT (ZII). x-right, y-up. Cartesian.  */
  COORD_ORIGIN_LEFT_TOP,        /* x-right, y-down. Screen/UI/web convention. */
  COORD_ORIGIN_RIGHT_BOTTOM,    /* x-left, y-up.  RTL + Cartesian.          */
  COORD_ORIGIN_RIGHT_TOP,       /* x-left, y-down.  RTL + screen convention.  */
  COORD_ORIGIN_CENTER,          /* x-up from center, y-right from center.     */
  COORD_ORIGIN_CUSTOM,          /* Game fills custom_* fields; dev owns axes. */
} CoordOrigin;

/* Configuration struct passed by the game to make_render_context().
 * ZII default (all zeros) → COORD_ORIGIN_LEFT_BOTTOM, custom_* unused.
 *
 * camera_zoom must be set explicitly; 0.0 produces a degenerate context
 * (all coords map to 0).  platform_game_props_init sets camera_zoom=1.0f. */
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  /* Only used when coord_origin == COORD_ORIGIN_CUSTOM: */
  float custom_x_offset; /* world x that maps to pixel x=0            */
  float custom_y_offset; /* world y that maps to pixel y=0            */
  float custom_x_sign;   /* +1.0 = x grows right (LTR), -1.0 = left  */
  float custom_y_sign;   /* +1.0 = y grows down,  -1.0 = y grows up  */
  /* Camera (pan + zoom).  Set explicitly; no ZII fallback for zoom.
   * camera_x : world x of the left edge of the visible viewport.
   * camera_y : world y of the bottom edge (y-up) or top edge (y-down).
   * camera_zoom: 1.0 = no zoom; 2.0 = zoom in 2×; 0.5 = zoom out 2×. */
  float camera_x;
  float camera_y;
  float camera_zoom;
} GameWorldConfig;

/* ── Origin-aware axis sign helpers ──────────────────────────────────────
 *
 * world_config_y_sign / world_config_x_sign
 * Return the axis direction (+1 or -1) for the given config.  Use these
 * whenever movement or camera-pan direction must be correct for ALL origins. */
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

/* ── Arena size constants ─────────────────────────────────────────────────
 *
 * Permanent arena: one-time allocations that live for the entire session.
 * Scratch arena: per-frame temporaries — reset each frame via TempMemory.
 */
#define ARENA_PERM_SIZE (1u * 1024u * 1024u) /* 1 MB  — session lifetime */
#define ARENA_SCRATCH_SIZE (256u * 1024u)    /* 256 KB — per-frame       */

#endif /* UTILS_BASE_H */
