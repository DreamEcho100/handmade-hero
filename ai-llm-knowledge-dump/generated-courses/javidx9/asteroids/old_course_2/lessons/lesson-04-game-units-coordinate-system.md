# Lesson 04 — Game Units and the Coordinate System

## What you'll learn

- Why game units exist (resolution independence)
- `GAME_W/H`, `GAME_UNITS_W/H`, and the 1-unit = 32-pixel relationship
- `CoordOrigin`, `GameWorldConfig`, `RenderContext`, `make_render_context()`
- `world_x/y/w/h()` — the explicit coordinate helpers
- `COORD_ORIGIN_LEFT_BOTTOM`: y-up, Cartesian (the ZII default)

---

## The problem: pixel constants

If game physics uses pixel values directly:

```c
#define SHIP_SPEED 400  // pixels per second
```

At 512px the ship feels fast. At 1024px it crawls. The game feel is
coupled to the canvas resolution — bad for resizable windows.

---

## The solution: game units

```c
// utils/base.h
#define GAME_W       512     // reference canvas width  (pixels)
#define GAME_H       480     // reference canvas height (pixels)
#define GAME_UNITS_W 16.0f   // canvas width  in game units
#define GAME_UNITS_H 15.0f   // canvas height in game units
```

1 game unit = `GAME_W / GAME_UNITS_W` = 32 pixels at reference resolution.

Physics constant conversion (÷32 from old pixel values):

| Old (px)  | New (units) | Constant              |
| --------- | ----------- | --------------------- |
| 200 px/s² | 6.25 f/s²   | `SHIP_THRUST_ACCEL`   |
| 220 px/s  | 6.875 f/s   | `SHIP_MAX_SPEED`      |
| 200 px/s  | 6.25 f/s    | `BULLET_SPEED`        |
| 48 px     | 1.5 units   | Large asteroid size   |
| 5 px      | 0.15625 u   | Ship collision radius |

---

## CoordOrigin

```c
typedef enum CoordOrigin {
    COORD_ORIGIN_LEFT_BOTTOM = 0,  // ZII default: y-up, x-right (Cartesian)
    COORD_ORIGIN_LEFT_TOP,          // x-right, y-down (screen/UI)
    // ... (RIGHT_BOTTOM, RIGHT_TOP, CENTER, CUSTOM)
} CoordOrigin;
```

`LEFT_BOTTOM` = zero-is-initialisation default. An unzeroed `GameWorldConfig`
automatically uses y-up Cartesian convention.

For Asteroids we use `COORD_ORIGIN_LEFT_BOTTOM`:

- `(0, 0)` = bottom-left of canvas
- x grows rightward
- y grows **upward**

---

## GameWorldConfig

```c
typedef struct GameWorldConfig {
    CoordOrigin coord_origin;  // axis directions
    float camera_x;            // world x of viewport left edge
    float camera_y;            // world y of viewport bottom (y-up)
    float camera_zoom;         // 1.0 = no zoom; must be set non-zero!
    // ... (custom_* fields for COORD_ORIGIN_CUSTOM)
} GameWorldConfig;
```

The platform initialises this and passes it to the game each frame:

```c
platform_game_props_init(&props);
// → coord_origin = COORD_ORIGIN_LEFT_BOTTOM
// → camera_zoom  = 1.0f
// → camera_x/y   = 0.0f
```

---

## RenderContext and make_render_context()

```c
// utils/render.h
typedef struct {
    float px_per_unit_x;   // pixels per game unit (x)
    float px_per_unit_y;   // pixels per game unit (y, may be negative for y-down)
    float origin_px_x;     // pixel x of world origin
    float origin_px_y;     // pixel y of world origin
    // + bb width/height for bounds checking
} RenderContext;

RenderContext make_render_context(const Backbuffer *bb,
                                  GameWorldConfig config);
```

`make_render_context` bakes world→pixel conversion coefficients once per
frame from `GameWorldConfig`. The hot path (draw calls) then has no branches.

For `COORD_ORIGIN_LEFT_BOTTOM` with a 512×480 canvas and zoom=1:

```
px_per_unit_x =  32.0  (512 / 16)
px_per_unit_y = -32.0  (480 / 15, negative = y-up flip)
origin_px_x   =  0.0
origin_px_y   = 480.0  (bottom of screen in pixel space)
```

---

## Explicit coordinate helpers

```c
// utils/render_explicit.h
// World → pixel conversions (all inline):
static inline float world_x(const RenderContext *ctx, float wx);
static inline float world_y(const RenderContext *ctx, float wy);
static inline float world_w(const RenderContext *ctx, float ww);
static inline float world_h(const RenderContext *ctx, float wh);
```

Example — drawing the ship at world position (8.0, 7.5):

```c
int px = (int)world_x(&world_ctx, ship->x);
int py = (int)world_y(&world_ctx, ship->y);
```

At reference resolution with zoom=1:

- `world_x(ctx, 8.0)` = 256 (screen centre)
- `world_y(ctx, 7.5)` = 240 (screen centre, after y-flip)

---

## Two RenderContexts per frame

```c
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config) {
    // World objects (ship, asteroids, bullets) — camera applied
    RenderContext world_ctx = make_render_context(bb, world_config);

    // HUD (score, game-over) — strip camera so HUD stays fixed
    GameWorldConfig hud_cfg  = world_config;
    hud_cfg.camera_x = 0.0f;
    hud_cfg.camera_y = 0.0f;
    hud_cfg.camera_zoom = 1.0f;
    RenderContext hud_ctx = make_render_context(bb, hud_cfg);

    // ... draw world objects with world_ctx
    // ... draw HUD with hud_ctx
}
```

---

## HUD_TOP_Y

```c
// utils/base.h
#define HUD_TOP_Y(offset_from_top) ((float)GAME_UNITS_H - (float)(offset_from_top))
```

In y-up world space, `y = GAME_UNITS_H` is the top of the screen.
`HUD_TOP_Y(0.25)` = `15 - 0.25` = `14.75` — 0.25 units below the top edge.

```c
TextCursor cur = make_cursor(&hud_ctx, 0.25f, HUD_TOP_Y(0.25f));
```

---

## Key takeaways

1. Game units decouple physics from resolution: ÷32 from pixel values.
2. `COORD_ORIGIN_LEFT_BOTTOM` is the ZII default (y-up, Cartesian).
3. `make_render_context()` bakes conversion coefficients once per frame.
4. `world_x/y()` convert game units to pixels with one multiply+add.
5. Two contexts per frame: `world_ctx` (camera) + `hud_ctx` (no camera).
6. `HUD_TOP_Y(offset)` = `GAME_UNITS_H - offset` for top-anchored HUD.
