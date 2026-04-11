# Lesson 01 — Project Setup, Window, and Backbuffer

## What you'll learn
- Directory layout: why `src/game/`, `src/platforms/`, `src/utils/`
- What a `Backbuffer` is and why we render into CPU memory first
- The pixel format (RGBA8888) and the `GAME_RGBA` macro
- Opening a Raylib window and uploading pixels each frame

---

## Directory layout

```
course/src/
  platform.h              ← platform/game contract (two-way API)
  game/
    game.h                ← game types and public API
    game.c                ← game logic (physics, rendering)
    audio.c               ← software audio mixer
  platforms/
    raylib/main.c         ← Raylib backend (~130 lines)
    x11/main.c            ← X11 + GLX + ALSA backend (~280 lines)
  utils/
    base.h                ← canvas size, game units, CoordOrigin
    backbuffer.h / .c     ← Backbuffer type, backbuffer_resize()
    render.h              ← RenderContext, make_render_context()
    render_explicit.h     ← world_x/y/w/h() coordinate helpers
    draw-shapes.h / .c    ← draw_rect(), draw_line()
    draw-text.h / .c      ← draw_char(), draw_text()
    math.h                ← Vec2, MIN/MAX/CLAMP
    audio.h               ← GameAudioState, SoundInstance, AudioOutputBuffer
```

**Rule:** `game/` never `#include`s anything from `platforms/`.
The dependency arrow goes: platform → game, never game → platform.

---

## The Backbuffer

A backbuffer is a plain array of pixels in CPU memory.  Every frame:

1. Game writes pixels into `pixels[]` via `draw_rect()` / `draw_text()`.
2. Platform uploads the array to the GPU (`glTexSubImage2D` / `UpdateTexture`).
3. GPU stretches the texture to fill the window (letterboxed).

```c
// utils/backbuffer.h
typedef struct {
    uint32_t *pixels;        // flat pixel array (platform allocates)
    int       width;         // canvas width in pixels (GAME_W)
    int       height;        // canvas height in pixels (GAME_H)
    int       pitch;         // bytes per row = width * bytes_per_pixel
    int       bytes_per_pixel; // 4 for RGBA8888
} Backbuffer;
```

JS analogy: `Uint32Array` backing a `<canvas>` `ImageData` object.

---

## Pixel format: RGBA8888

```c
#define GAME_RGBA(r, g, b, a) \
    ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
      ((uint32_t)(g) <<  8) |  (uint32_t)(r) )
```

On a little-endian CPU (x86/ARM), the in-memory bytes are `[R][G][B][A]`
(byte 0 = R, byte 3 = A).  This matches OpenGL `GL_RGBA` and Raylib's
`PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` exactly — no byte-swapping needed.

```c
uint32_t red   = GAME_RGBA(255, 0,   0,   255);
uint32_t white = GAME_RGBA(255, 255, 255, 255);
uint32_t black = GAME_RGBA(0,   0,   0,   255);
```

---

## Canvas size

```c
// utils/base.h
#define GAME_W 512
#define GAME_H 480
```

`512 × 480` is the classic Asteroids feel — wide but not quite 4:3.
The platform allocates `GAME_W * GAME_H` pixels and points `Backbuffer.pixels` at them.

---

## Raylib backend: opening a window

```c
// platforms/raylib/main.c (key parts)

static uint32_t g_pixel_buf[GAME_W * GAME_H];

void platform_init(void) {
    InitWindow(GAME_W, GAME_H, "Asteroids");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    Image img = {
        .data    = g_pixel_buf,
        .width   = GAME_W,
        .height  = GAME_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);

    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME);
    g_audio_stream = LoadAudioStream(44100, 16, 2);
    PlayAudioStream(g_audio_stream);
}
```

`FLAG_WINDOW_RESIZABLE` lets users drag the window to any size.  The game
canvas (`GAME_W × GAME_H`) stays fixed; we letterbox it into the window.

---

## Uploading pixels each frame

```c
// In the main loop:
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

`platform_compute_letterbox()` computes the centered, aspect-preserving
viewport.  `DrawTexturePro` stretches the texture into that viewport.
`ClearBackground(BLACK)` fills the rest of the window (the bars) with black.

---

## PlatformGameProps

```c
// platform.h
typedef struct {
    Backbuffer      backbuffer;
    GameWorldConfig world_config;
    WindowScaleMode scale_mode;
} PlatformGameProps;
```

Instead of threading backbuffer and world config through every function call,
the platform packs them into a single struct.  `platform_game_props_init()`
sets sensible defaults (y-up origin, zoom=1, fixed scale mode):

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

## Key takeaways

1. `Backbuffer` is a plain CPU array — no GPU objects in game code.
2. RGBA8888 byte layout matches GL_RGBA and Raylib with no swapping.
3. `GAME_W × GAME_H = 512 × 480` is the fixed game canvas.
4. `FLAG_WINDOW_RESIZABLE` + letterbox = any window size, same game feel.
5. `PlatformGameProps` bundles backbuffer + world config for clean passing.
