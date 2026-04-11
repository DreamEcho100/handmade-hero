#ifndef UTILS_BASE_H
#define UTILS_BASE_H

/* ── Canvas dimensions ────────────────────────────────────────────────── */
/* 320x180 pixel-art resolution, matching the original SlimeEscape viewport.
 * Upscaled 3x to 960x540 via GPU letterbox.                              */
#define GAME_W 320
#define GAME_H 180

/* ── Game-world unit system ───────────────────────────────────────────── */
/* 1:1 ratio: one game unit = one pixel at native resolution.
 * All position constants from the original GDScript transfer directly.
 * GAME_UNITS always applies — with 1:1 the math is trivially transparent. */
#define GAME_UNITS_W 320.0f
#define GAME_UNITS_H 180.0f

/* HUD_TOP_Y — convert an offset-from-top to a world-y coordinate.
 * With COORD_ORIGIN_LEFT_TOP (y-down), y=0 is screen top.
 * HUD_TOP_Y(offset) simply returns the offset from the top.              */
#define HUD_TOP_Y(offset_from_top) ((float)(offset_from_top))

/* HUD_BOTTOM_Y — anchor to bottom of screen. */
#define HUD_BOTTOM_Y(offset_from_bottom)                                       \
  ((float)GAME_UNITS_H - (float)(offset_from_bottom))

/* ── Scaling Mode ─────────────────────────────────────────────────────── */
typedef enum {
  WINDOW_SCALE_MODE_FIXED,         /* Fixed 320x180 + GPU letterbox scaling */
  WINDOW_SCALE_MODE_DYNAMIC_MATCH, /* Backbuffer matches window (can distort) */
  WINDOW_SCALE_MODE_DYNAMIC_ASPECT, /* Backbuffer resizes, preserves 16:9 */
  WINDOW_SCALE_MODE_COUNT
} WindowScaleMode;

/* ── Coordinate origin ────────────────────────────────────────────────── */
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM =
      0,                     /* DEFAULT (ZII). x-right, y-up. HH/Cartesian.  */
  COORD_ORIGIN_LEFT_TOP,     /* x-right, y-down. Screen/UI/web convention.    */
  COORD_ORIGIN_RIGHT_BOTTOM, /* x-left, y-up.  RTL + Cartesian.             */
  COORD_ORIGIN_RIGHT_TOP,    /* x-left, y-down.  RTL + screen convention.     */
  COORD_ORIGIN_CENTER,       /* x-up from center, y-right from center.        */
  COORD_ORIGIN_CUSTOM, /* Game fills custom_* fields; dev owns the axes. */
} CoordOrigin;

/* GameWorldConfig — passed to make_render_context() once per frame.
 * For this game: COORD_ORIGIN_LEFT_TOP (y-down, matching Godot).          */
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  /* Only used when coord_origin == COORD_ORIGIN_CUSTOM: */
  float custom_x_offset;
  float custom_y_offset;
  float custom_x_sign;
  float custom_y_sign;
  /* Camera (pan + zoom). */
  float camera_x;
  float camera_y;
  float camera_zoom;
} GameWorldConfig;

/* Origin-aware axis sign helpers. */
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

/* ── Arena size constants ─────────────────────────────────────────────── */
/* Enlarged perm arena for game state + entity pools.
 * Scratch arena is standard 256 KB for per-frame temporaries.             */
#define ARENA_PERM_SIZE (4u * 1024u * 1024u) /* 4 MB  — session lifetime */
#define ARENA_SCRATCH_SIZE (256u * 1024u)    /* 256 KB — per-frame       */

#endif /* UTILS_BASE_H */
