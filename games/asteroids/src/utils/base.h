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
  WINDOW_SCALE_MODE_FIXED,         /* Fixed 800×600 + GPU letterbox scaling  */
  WINDOW_SCALE_MODE_DYNAMIC_MATCH, /* Backbuffer matches window (can distort) */
  WINDOW_SCALE_MODE_DYNAMIC_ASPECT, /* Backbuffer resizes but preserves 4:3   */
  WINDOW_SCALE_MODE_COUNT
} WindowScaleMode;

/* ── Coordinate origin ─────────────────────────────────────────────────── */
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM = 0, /* DEFAULT (ZII). x-right, y-up. Cartesian.  */
  COORD_ORIGIN_LEFT_TOP,        /* x-right, y-down. Screen/UI/web convention. */
  COORD_ORIGIN_RIGHT_BOTTOM,    /* x-left, y-up.  RTL + Cartesian.          */
  COORD_ORIGIN_RIGHT_TOP,       /* x-left, y-down.  RTL + screen convention.  */
  COORD_ORIGIN_CENTER,          /* x-up from center, y-right from center.     */
  COORD_ORIGIN_CUSTOM,          /* Game fills custom_* fields; dev owns axes. */
} CoordOrigin;

/* Configuration passed to make_render_context() once per frame.
 * ZII default (all zeros) → COORD_ORIGIN_LEFT_BOTTOM. */
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  float custom_x_offset; /* world x that maps to pixel x=0            */
  float custom_y_offset; /* world y that maps to pixel y=0            */
  float custom_x_sign;   /* +1.0 = LTR (x grows right), -1.0 = RTL   */
  float custom_y_sign;   /* +1.0 = y grows down, -1.0 = y grows up   */
} GameWorldConfig;

#endif // UTILS_BASE_H