# Lesson 06 — The Platform Contract

## What you'll learn
- Dependency inversion: why game code must never include platform headers
- `PlatformGameProps` and `platform_game_props_init()`
- The four platform → game functions
- `platform_compute_letterbox()` for aspect-preserving display

---

## Dependency inversion

```
platform/raylib/main.c  ─── includes ──▶  platform.h
platform/x11/main.c     ─── includes ──▶  platform.h
                                             │
                                             ▼
                                        game/game.h
                                        utils/*.h
                                        (NO X11, NO raylib)
```

Game code (`game/game.c`, `game/audio.c`) includes only:
- `game/game.h` (game types)
- `utils/render.h`, `utils/draw-shapes.h`, `utils/draw-text.h`

It **never** includes `raylib.h`, `X11/Xlib.h`, or `GL/gl.h`.
Platforms depend on the game — never the other way around.

---

## platform.h — the two-way contract

```c
// src/platform.h

#include "game/game.h"

// Platform-owned data the platform passes to the game each frame
typedef struct {
    Backbuffer      backbuffer;   // pixel buffer the game renders into
    GameWorldConfig world_config; // coordinate system config
    WindowScaleMode scale_mode;   // how platform scales to window
} PlatformGameProps;

// platform → game (game/game.c exports these)
void asteroids_init(GameState *state);
void asteroids_update(GameState *state, const GameInput *input, float dt);
void asteroids_render(const GameState *state, Backbuffer *bb,
                      GameWorldConfig world_config);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out);

// game → platform (platforms export these)
void platform_init(void);
void platform_shutdown(void);
```

---

## platform_game_props_init()

```c
static inline void platform_game_props_init(PlatformGameProps *props) {
    if (!props) return;
    Backbuffer zero_bb = {0};
    props->backbuffer  = zero_bb;

    GameWorldConfig cfg = {0};
    cfg.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;
    cfg.camera_zoom  = 1.0f;
    cfg.camera_x     = 0.0f;
    cfg.camera_y     = 0.0f;
    props->world_config = cfg;

    props->scale_mode = WINDOW_SCALE_MODE_FIXED;
}
```

After calling this, the platform fills in backbuffer fields:

```c
PlatformGameProps props;
platform_game_props_init(&props);

props.backbuffer.pixels          = g_pixel_buf;
props.backbuffer.width           = GAME_W;
props.backbuffer.height          = GAME_H;
props.backbuffer.pitch           = GAME_W * 4;
props.backbuffer.bytes_per_pixel = 4;
```

---

## The four platform → game calls

Each frame, the platform calls them in this order:

```c
// 1. (done once at startup)
asteroids_init(&state);

// Per frame:
// 2. Update simulation
asteroids_update(&state, &inputs[1], dt);

// 3. Render into backbuffer
asteroids_render(&state, &props.backbuffer, props.world_config);

// 4. Fill audio output (when ready)
game_get_audio_samples(&state, &audio_buf);
```

**Why pass `world_config` to render?**
The platform owns the camera and coordinate origin.  Passing `world_config`
lets the platform decide zoom, pan, and even change `coord_origin` at runtime
without the game needing to know which platform it's running on.

---

## platform_compute_letterbox()

```c
// platform.h (inline)
static inline void platform_compute_letterbox(
    int game_w, int game_h,
    int win_w,  int win_h,
    int *out_x, int *out_y,
    int *out_w, int *out_h)
{
    float sx    = (float)win_w / (float)game_w;
    float sy    = (float)win_h / (float)game_h;
    float scale = (sx < sy) ? sx : sy;
    *out_w = (int)((float)game_w * scale);
    *out_h = (int)((float)game_h * scale);
    *out_x = (win_w - *out_w) / 2;
    *out_y = (win_h - *out_h) / 2;
}
```

`min(win_w/game_w, win_h/game_h)` picks the smaller scale factor so the
canvas fits entirely within the window.  The remaining space becomes black bars.

Shared between both platforms via a single definition in `platform.h`.

---

## Platform-specific render loop (Raylib)

```c
// Render once
asteroids_render(&state, &props.backbuffer, props.world_config);

// Upload and display
UpdateTexture(g_texture, g_pixel_buf);

int sw = GetScreenWidth(), sh = GetScreenHeight();
int vx, vy, vw, vh;
platform_compute_letterbox(GAME_W, GAME_H, sw, sh, &vx, &vy, &vw, &vh);

BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(g_texture,
        (Rectangle){0, 0, (float)GAME_W, (float)GAME_H},
        (Rectangle){(float)vx, (float)vy, (float)vw, (float)vh},
        (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
EndDrawing();
```

---

## Key takeaways

1. Game code never includes platform headers — dependency flows one way.
2. `PlatformGameProps` bundles backbuffer + world config + scale mode.
3. `platform_game_props_init()` sets safe defaults (y-up, zoom=1, fixed scale).
4. The platform owns `world_config`; it passes it into `asteroids_render()`.
5. `platform_compute_letterbox()` is shared inline in `platform.h`.
