#ifndef UTILS_BASE_H
#define UTILS_BASE_H

/* ── Canvas dimensions ────────────────────────────────────────────────── */
#define GAME_W 800
#define GAME_H 600

/* ── Game-world unit system ───────────────────────────────────────────── */
#define GAME_UNITS_W 16.0f
#define GAME_UNITS_H 12.0f

/* ── Scaling Mode ─────────────────────────────────────────────────────── */
typedef enum {
  WINDOW_SCALE_MODE_FIXED,         /* Fixed 800×600 + GPU letterbox scaling */
  WINDOW_SCALE_MODE_DYNAMIC_MATCH, /* Backbuffer matches window (can distort) */
  WINDOW_SCALE_MODE_DYNAMIC_ASPECT, /* Backbuffer resizes but preserves 4:3 */
  WINDOW_SCALE_MODE_COUNT
} WindowScaleMode;

/* ── Coordinate origin — LESSON 09 ───────────────────────────────────────
 *
 * Selects where world-space (0,0) maps to in the window, and which direction
 * each axis grows.  Passed to make_render_context() once per frame; baked
 * into RenderContext floats so the hot path (draw calls) has no branches.
 *
 * NAMING CONVENTION — horizontal qualifier first:
 *   "LEFT_BOTTOM"  = origin is at the bottom-left corner.
 *   "RIGHT_TOP"    = origin is at the top-right corner.
 * The first word names the x-axis side (LEFT/RIGHT);
 * the second word names the y-axis side (BOTTOM/TOP).
 *
 * ZII: COORD_ORIGIN_LEFT_BOTTOM = 0 — uninitialized GameWorldConfig structs
 * automatically get the Handmade Hero / Cartesian default.
 *
 * Standard 2×2 matrix of axis combinations:
 *
 * | Origin        | x grows | y grows | (0,0) at      | Use case |
 * |---------------|---------|---------|---------------|------------------------------|
 * | BOTTOM_LEFT   | right   | up      | bottom-left   | HH, physics,
 * platformers     | | TOP_LEFT      | right   | down    | top-left      | UI,
 * text, pixel-art, web     | | BOTTOM_RIGHT  | left    | up      | bottom-right
 * | RTL + Cartesian (Arabic etc) | | TOP_RIGHT     | left    | down    |
 * top-right     | RTL + screen convention      | | CENTER        | right   | up
 * | screen center | Asteroids, twin-stick shmups | | CUSTOM        | either  |
 * either  | anywhere      | Sub-region, any axes         |
 *
 * COORD_ORIGIN_CUSTOM — developer responsibility:
 * ────────────────────────────────────────────────
 * Fill GameWorldConfig.custom_* fields directly.
 *   custom_x_offset = world x that maps to pixel x=0
 *   custom_y_offset = world y that maps to pixel y=0
 *   custom_x_sign   = +1 (x grows right, LTR) or -1 (x grows left, RTL)
 *   custom_y_sign   = +1 (y grows down)        or -1 (y grows up)
 *
 * world_config_x_sign() / world_config_y_sign() forward custom_x/y_sign
 * as-is.  The helpers have no knowledge of CUSTOM axes — the developer owns
 * all direction math.  camera_pan() and screen_move() work correctly only
 * if custom_x_sign / custom_y_sign match the convention you intended.
 *
 * You can call make_render_context() twice per frame with different configs
 * (e.g. LTR world + RTL UI panel).  The branch cost is paid once per call;
 * draw calls are always branch-free float math.
 */
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM =
      0,                     /* DEFAULT (ZII). x-right, y-up. HH/Cartesian.  */
  COORD_ORIGIN_LEFT_TOP,     /* x-right, y-down. Screen/UI/web convention.    */
  COORD_ORIGIN_RIGHT_BOTTOM, /* x-left, y-up.  RTL + Cartesian.             */
  COORD_ORIGIN_RIGHT_TOP,    /* x-left, y-down.  RTL + screen convention.     */
  COORD_ORIGIN_CENTER,       /* x-up from center, y-right from center.        */
  COORD_ORIGIN_CUSTOM, /* Game fills custom_* fields; dev owns the axes. */
} CoordOrigin;

/* Configuration struct passed by the game to make_render_context().
 * ZII default (all zeros) → COORD_ORIGIN_LEFT_BOTTOM, custom_* unused.
 *
 * LESSON 17 — Camera fields added.  camera_zoom must be set explicitly;
 * 0.0 produces a degenerate context (all coords map to 0).  The platform
 * sets camera_zoom=1.0f in platform_game_props_init as the safe default.  */
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  /* Only used when coord_origin == COORD_ORIGIN_CUSTOM: */
  float custom_x_offset; /* world x that maps to pixel x=0            */
  float custom_y_offset; /* world y that maps to pixel y=0            */
  float custom_x_sign;   /* +1.0 = x grows right (LTR), -1.0 = left (RTL) */
  float custom_y_sign;   /* +1.0 = y grows down,  -1.0 = y grows up   */
  /* LESSON 17 — Camera (pan + zoom).  Set explicitly; no ZII fallback.
   * camera_x : world x of the left edge of the visible viewport.
   * camera_y : world y of the bottom edge (y-up) or top edge (y-down).
   * camera_zoom: 1.0 = no zoom; 2.0 = zoom in 2×; 0.5 = zoom out 2×. */
  float camera_x;
  float camera_y;
  float camera_zoom;
} GameWorldConfig;

/* ── Origin-aware axis sign helpers — LESSON 17 ──────────────────────────
 *
 * world_config_y_sign / world_config_x_sign
 * ──────────────────────────────────────────
 * Return the axis direction (+1 or -1) for the given config.  Use these
 * whenever movement or camera-pan direction must be correct for ALL origins,
 * not just TOP_LEFT.
 *
 * Why this is needed — pixel-coordinate asymmetry:
 *   pixel_x decreasing → content moves LEFT
 *   pixel_y decreasing → content moves UP
 * The same "+= speed" applied to both camera fields therefore produces
 * OPPOSITE visible directions on X vs Y.  Multiplying by the axis sign
 * absorbs this asymmetry in one place.
 *
 * COORD_ORIGIN_CUSTOM: the helper forwards custom_y_sign / custom_x_sign
 * directly.  The developer who chose CUSTOM is responsible for setting those
 * fields correctly — the helper just passes them through.                   */
static inline float world_config_y_sign(const GameWorldConfig *cfg) {
  if (cfg->coord_origin == COORD_ORIGIN_CUSTOM)
    return cfg->custom_y_sign;
  /* TOP_LEFT and TOP_RIGHT: y grows down (+1). All others: y grows up (-1). */
  return (cfg->coord_origin == COORD_ORIGIN_LEFT_TOP ||
          cfg->coord_origin == COORD_ORIGIN_RIGHT_TOP)
             ? 1.0f
             : -1.0f;
}

static inline float world_config_x_sign(const GameWorldConfig *cfg) {
  if (cfg->coord_origin == COORD_ORIGIN_CUSTOM)
    return cfg->custom_x_sign;
  /* BOTTOM_RIGHT and TOP_RIGHT: x grows left (-1, RTL). All others: +1. */
  return (cfg->coord_origin == COORD_ORIGIN_RIGHT_BOTTOM ||
          cfg->coord_origin == COORD_ORIGIN_RIGHT_TOP)
             ? -1.0f
             : 1.0f;
}

/* ── Arena size constants — LESSON 16 ────────────────────────────────────
 *
 * Permanent arena: one-time allocations that live for the entire session
 * (tilemap data, loaded assets, entity tables).
 *
 * Scratch arena: per-frame temporaries — reset each frame via TempMemory.
 * See platform.h and arena.h for the TempMemory checkpoint pattern.
 */
#define ARENA_PERM_SIZE (1u * 1024u * 1024u) /* 1 MB  — session lifetime */
#define ARENA_SCRATCH_SIZE (256u * 1024u)    /* 256 KB — per-frame       */

#endif /* UTILS_BASE_H */
