# Lesson 02: Drawing Primitives

## What we're building

`draw_rect` fills a clipped rectangle with a solid colour.  `draw_rect_blend`
does the same but respects the colour's alpha byte to blend with whatever is
already in the backbuffer.  Together they are the only pixel-writing primitives
the entire game uses until we add sprite rendering in Lesson 08.

We also run the **pure-red cross-backend validation test**: a full-screen red
rectangle confirmed to look identical on both X11 and Raylib.  Passing this test
means `GAME_RGBA` byte ordering is wired up correctly end-to-end.

---

## What you'll learn

- How `GAME_RGBA(r,g,b,a) = (a<<24)|(b<<16)|(g<<8)|r` packs four bytes
- Why `0xAABBGGRR` stores R in the low byte on little-endian CPUs
- How to extract individual colour channels with shifts and masks
- Alpha blending with integer arithmetic: `out = (src*a + dst*(255-a)) / 255`
- `CONSOLE_PALETTE[16][3]` — the 16-colour Windows console palette
- Named `COLOR_*` constants — why they beat bare `GAME_RGBA` literals
- The pure-red test and what it catches when the byte order is wrong

---

## Prerequisites

- Lesson 01 complete: window opens, `Backbuffer` struct defined, `GAME_RGBA` macro
- Understand row-major indexing: `pixels[y * (pitch/4) + x]`

---

## Concepts

### 1. Bit-Packing: `GAME_RGBA`

Each pixel is a `uint32_t`.  We pack four 8-bit channels into it:

```c
#define GAME_RGBA(r, g, b, a) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | (uint32_t)(r))
```

The bit layout in a 32-bit register:

```
bit 31 ──────────────────────────────────── bit 0
[ A: 24–31 ][ B: 16–23 ][ G: 8–15 ][ R: 0–7 ]
```

On a **little-endian** CPU (x86/ARM in LE mode), when this value sits in memory,
the **lowest address holds the lowest byte** — which is R:

```
address:    base+0  base+1  base+2  base+3
byte:       R       G       B       A
```

This is exactly what OpenGL expects when you call
`glTexImage2D(…, GL_RGBA, GL_UNSIGNED_BYTE, pixels)`:
it reads bytes in memory order and treats the first byte as R.

```c
// JS equivalent using a DataView (explicit little-endian):
// function rgba(r, g, b, a) {
//   const buf = new Uint8Array([r, g, b, a]);
//   return new DataView(buf.buffer).getUint32(0, /*littleEndian=*/true);
// }
// rgba(255, 0, 0, 255)  // → 0xFF0000FF
```

> **Course Note:** The reference Frogger source uses `FROGGER_RGBA` — a
> different name for the same formula and the same byte layout.  The course
> renames it `GAME_RGBA` for consistency with the Tetris/Snake/Asteroids courses.
> `FROGGER_RGBA(r,g,b,a)` and `GAME_RGBA(r,g,b,a)` produce identical values.
> No pixel data is different; only the macro name changes.

### 2. Extracting Channels

To read R, G, B, A back out of a packed `uint32_t`:

```c
uint32_t color = GAME_RGBA(100, 200, 50, 128);

uint8_t r = (uint8_t)( color         & 0xFF);  /* bits  0–7  */
uint8_t g = (uint8_t)((color >>  8)  & 0xFF);  /* bits  8–15 */
uint8_t b = (uint8_t)((color >> 16)  & 0xFF);  /* bits 16–23 */
uint8_t a = (uint8_t)((color >> 24)  & 0xFF);  /* bits 24–31 */

// JS equivalent:
// const r = color & 0xFF;
// const g = (color >>> 8) & 0xFF;     // >>> = unsigned right-shift in JS
// const b = (color >>> 16) & 0xFF;
// const a = (color >>> 24) & 0xFF;
```

Note: JS uses `>>>` (unsigned right-shift) whereas C `>>` on `uint32_t` already
gives the correct unsigned result.

### 3. Alpha Blending Formula

The Porter-Duff "source over destination" blend:

```
result = (src * a + dst * (255 - a)) / 255
```

Applied per-channel in integer arithmetic:

```c
uint8_t blend_channel(uint8_t src, uint8_t dst, uint8_t a) {
    return (uint8_t)((src * a + dst * (255 - a)) / 255);
}
```

```c
// JS equivalent:
// function blendChannel(src, dst, alpha) {
//   return Math.round((src * alpha + dst * (255 - alpha)) / 255);
// }

// Example: src = red (255,0,0), alpha = 128 (50%), dst = green (0,255,0)
// R: (255*128 + 0*127) / 255 ≈ 128
// G: (  0*128 + 255*127) / 255 ≈ 127
// Result: muddy orange-ish — exactly what you'd get with ctx.globalAlpha=0.5
```

### 4. `CONSOLE_PALETTE` — The 16-Colour Windows Palette

Frogger's sprites were originally designed for the Windows console using 4-bit
colour indices (0–15).  The `CONSOLE_PALETTE` table maps each index to an
`{R, G, B}` triple:

```c
// From game.h — the same 16 colours the original game used:
static const uint8_t CONSOLE_PALETTE[16][3] = {
    {  0,   0,   0},  /* 0  Black        */
    {  0,   0, 128},  /* 1  Dark Blue    */
    {  0, 128,   0},  /* 2  Dark Green   */
    {  0, 128, 128},  /* 3  Dark Cyan    */
    {128,   0,   0},  /* 4  Dark Red     */
    {128,   0, 128},  /* 5  Dark Magenta */
    {128, 128,   0},  /* 6  Dark Yellow  */
    {192, 192, 192},  /* 7  Gray         */
    {128, 128, 128},  /* 8  Dark Gray    */
    {  0,   0, 255},  /* 9  Blue         */
    {  0, 255,   0},  /* 10 Bright Green */
    {  0, 255, 255},  /* 11 Cyan         */
    {255,   0,   0},  /* 12 Red          */
    {255,   0, 255},  /* 13 Magenta      */
    {255, 255,   0},  /* 14 Yellow       */
    {255, 255, 255},  /* 15 White        */
};
```

We use this palette in Lesson 08 to convert sprite color indices to `GAME_RGBA`
values.  For now it lives in `game.h` for reference.

```c
// JS analogy: an enum-indexed CSS color map:
// const CONSOLE_PALETTE = [
//   '#000000', '#000080', '#008000', /* ... */
// ];
// const color = CONSOLE_PALETTE[colorIndex];
```

### 5. Named `COLOR_*` Constants

Instead of scattering `GAME_RGBA(0,0,0,255)` everywhere, we define names:

```c
#define COLOR_BLACK        GAME_RGBA(  0,   0,   0, 255)
#define COLOR_WHITE        GAME_RGBA(255, 255, 255, 255)
#define COLOR_RED          GAME_RGBA(255,   0,   0, 255)
#define COLOR_GREEN        GAME_RGBA(  0, 255,   0, 255)
#define COLOR_YELLOW       GAME_RGBA(255, 255,   0, 255)
#define COLOR_CYAN         GAME_RGBA(  0, 255, 255, 255)
#define COLOR_OVERLAY_DIM  GAME_RGBA(  0,   0,   0, 160)  /* semi-transparent */
```

```c
// JS equivalent:
// const COLORS = {
//   BLACK:   'rgba(0, 0, 0, 1)',
//   WHITE:   'rgba(255, 255, 255, 1)',
//   OVERLAY: 'rgba(0, 0, 0, 0.63)',   // 160/255 ≈ 0.63
// };
```

> **Note:** These are `#define` macros, not `const` variables — they expand at
> every use site with zero runtime overhead.

### 6. The Pure-Red Validation Test

The single most common mistake when setting up a new pixel-writing pipeline is
swapping R and B.  If you draw `GAME_RGBA(255, 0, 0, 255)` (pure red) and see
**cyan** or **blue**, the byte order is wrong.

The test:

```c
// In your render function, before any real game drawing:
draw_rect(bb, 0, 0, bb->width, bb->height, GAME_RGBA(255, 0, 0, 255));
```

Expected: full-screen red on **both** X11 and Raylib.

- If X11 shows cyan: `GL_BGRA` was used instead of `GL_RGBA` in `glTexImage2D`
- If Raylib shows the wrong colour: `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` is wrong

---

## Step 1 — `src/utils/draw-shapes.h`

```c
/* utils/draw-shapes.h — Primitive Drawing */
#ifndef FROGGER_DRAW_SHAPES_H
#define FROGGER_DRAW_SHAPES_H

#include <stdint.h>
#include "backbuffer.h"

/* Solid filled rectangle, clamped to buffer bounds.
   JS: ctx.fillStyle = css; ctx.fillRect(x, y, w, h);                    */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* Alpha-blended rectangle.  Uses colour's high byte (bits 24–31) as alpha.
   alpha=255 → fully opaque; alpha=128 → 50% blend; alpha=0 → no-op.
   JS: ctx.globalAlpha = alpha/255; ctx.fillRect(x, y, w, h);            */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color);

/* UV-cropped sprite partial — added in Lesson 08.
   Declared here so draw-shapes.h doesn't need a separate header later.  */
void draw_sprite_partial(
    Backbuffer *bb,
    const int16_t *colors, const int16_t *glyphs,
    const uint8_t palette[16][3],
    int sheet_w,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y);

#endif /* FROGGER_DRAW_SHAPES_H */
```

---

## Step 2 — `src/utils/draw-shapes.c`

```c
/* utils/draw-shapes.c — Primitive Drawing Implementation */
#define _POSIX_C_SOURCE 200809L
#include "draw-shapes.h"
#include <stdint.h>

/* ── draw_rect ─────────────────────────────────────────────────────────────
   Solid filled rectangle, clamped to buffer bounds.

   Clamping (not skipping): if the rect partially overlaps the screen we
   still draw the visible portion — useful for tiles at lane edges.

   stride = pitch / 4:  converts byte-stride to uint32_t element stride.
   With pitch = width * 4, stride == width.  We keep the formula explicit
   for GPU-alignment safety (see Lesson 01 on pitch).

   JS equivalent:
     ctx.fillStyle = colorToCSS(color);
     ctx.fillRect(x, y, w, h);                                           */
void draw_rect(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;

    int row, col;
    int stride = bb->pitch / 4;
    for (row = y0; row < y1; row++)
        for (col = x0; col < x1; col++)
            bb->pixels[row * stride + col] = color;
}

/* ── draw_rect_blend ───────────────────────────────────────────────────────
   Alpha-blended rectangle using Porter-Duff "source over destination":
     out = (src * alpha + dst * (255 - alpha)) / 255

   Channel extraction uses our GAME_RGBA layout (R = low byte):
     R = bits  0–7   → (color >>  0) & 0xFF
     G = bits  8–15  → (color >>  8) & 0xFF
     B = bits 16–23  → (color >> 16) & 0xFF
     A = bits 24–31  → (color >> 24) & 0xFF

   JS equivalent:
     ctx.globalAlpha = alpha / 255;
     ctx.fillRect(x, y, w, h);
     ctx.globalAlpha = 1.0;                                              */
void draw_rect_blend(Backbuffer *bb, int x, int y, int w, int h, uint32_t color) {
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + w > bb->width)  ? bb->width  : x + w;
    int y1 = (y + h > bb->height) ? bb->height : y + h;

    uint8_t sa = (uint8_t)((color >> 24) & 0xFF);  /* source alpha      */
    uint8_t sb = (uint8_t)((color >> 16) & 0xFF);  /* source B          */
    uint8_t sg = (uint8_t)((color >>  8) & 0xFF);  /* source G          */
    uint8_t sr = (uint8_t)( color        & 0xFF);  /* source R          */

    int row, col;
    int stride = bb->pitch / 4;
    for (row = y0; row < y1; row++) {
        for (col = x0; col < x1; col++) {
            uint32_t dst = bb->pixels[row * stride + col];
            uint8_t db = (uint8_t)((dst >> 16) & 0xFF);
            uint8_t dg = (uint8_t)((dst >>  8) & 0xFF);
            uint8_t dr = (uint8_t)( dst        & 0xFF);

            uint8_t or_ = (uint8_t)((sr * sa + dr * (255 - sa)) / 255);
            uint8_t og  = (uint8_t)((sg * sa + dg * (255 - sa)) / 255);
            uint8_t ob  = (uint8_t)((sb * sa + db * (255 - sa)) / 255);

            bb->pixels[row * stride + col] = GAME_RGB(or_, og, ob);
        }
    }
}

/* ── draw_sprite_partial ───────────────────────────────────────────────────
   Implemented in full in Lesson 08.  Stub here so the project compiles
   from Lesson 02 onward.

   The real implementation renders a UV-cropped region of a sprite sheet:
     src_x/src_y/src_w/src_h: source region in cells (each cell = CELL_PX px)
     dest_px_x/dest_px_y:     destination in pixels

   JS equivalent:
     ctx.drawImage(sheet, sx*8, sy*8, sw*8, sh*8, dx, dy, sw*8, sh*8);  */
void draw_sprite_partial(
    Backbuffer *bb,
    const int16_t *colors, const int16_t *glyphs,
    const uint8_t palette[16][3],
    int sheet_w,
    int src_x, int src_y, int src_w, int src_h,
    int dest_px_x, int dest_px_y)
{
    /* Full implementation in Lesson 08 */
    (void)bb; (void)colors; (void)glyphs; (void)palette; (void)sheet_w;
    (void)src_x; (void)src_y; (void)src_w; (void)src_h;
    (void)dest_px_x; (void)dest_px_y;
}
```

---

## Step 3 — Updated Raylib `main_raylib.c` (Lesson 02 stage)

Add the draw calls to the render section.  Everything else is identical to
Lesson 01.

```c
/* main_raylib.c — Raylib Backend (Lesson 02: drawing primitives) */
#include <string.h>
#include <stdint.h>
#include "raylib.h"
#include "platform.h"
#include "utils/draw-shapes.h"

static Texture2D g_texture;
static uint32_t  g_pixel_buf[SCREEN_W * SCREEN_H];

int platform_init(PlatformGameProps *props) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(props->width, props->height, props->title);
    SetTargetFPS(60);
    Image img = { .data = g_pixel_buf, .width = SCREEN_W, .height = SCREEN_H,
                  .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);
    props->backbuffer.pixels = g_pixel_buf;
    props->backbuffer.width  = SCREEN_W;
    props->backbuffer.height = SCREEN_H;
    props->backbuffer.pitch  = SCREEN_W * 4;
    props->is_running = 1;
    return 1;
}

double platform_get_time(void) { return GetTime(); }

void platform_get_input(GameInput *input, PlatformGameProps *props) {
    if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        input->quit = 1; props->is_running = 0;
    }
}

void platform_shutdown(void) { UnloadTexture(g_texture); CloseWindow(); }

int main(void) {
    PlatformGameProps props;
    memset(&props, 0, sizeof(props));
    props.title  = "Frogger";
    props.width  = SCREEN_W;
    props.height = SCREEN_H;
    if (!platform_init(&props)) return 1;

    GameInput inputs[2];
    memset(inputs, 0, sizeof(inputs));

    while (!WindowShouldClose() && props.is_running) {
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        Backbuffer *bb = &props.backbuffer;

        /* ── Pure-red validation test ───────────────────────────────── */
        /* Expected: full-screen red background with coloured squares.
           If you see cyan or blue instead of red, GL_BGRA vs GL_RGBA    */
        draw_rect(bb, 0, 0, bb->width, bb->height,
                  GAME_RGBA(200, 0, 0, 255));   /* deep red background */

        draw_rect(bb, 20, 20, 120, 80,
                  COLOR_WHITE);                  /* white rectangle     */

        draw_rect(bb, 160, 20, 120, 80,
                  COLOR_GREEN);                  /* bright green        */

        draw_rect(bb, 300, 20, 120, 80,
                  COLOR_YELLOW);                 /* yellow              */

        draw_rect(bb, 440, 20, 120, 80,
                  COLOR_CYAN);                   /* cyan                */

        /* Semi-transparent dark overlay over the right half of the screen */
        draw_rect_blend(bb, bb->width / 2, 0, bb->width / 2, bb->height,
                        COLOR_OVERLAY_DIM);       /* alpha = 160        */

        /* Upload + display */
        UpdateTexture(g_texture, g_pixel_buf);
        {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float sc = ((float)sw / SCREEN_W < (float)sh / SCREEN_H)
                       ? (float)sw / SCREEN_W : (float)sh / SCREEN_H;
            int vw = (int)(SCREEN_W * sc), vh = (int)(SCREEN_H * sc);
            int vx = (sw - vw) / 2,        vy = (sh - vh) / 2;
            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(g_texture,
                (Rectangle){0, 0, SCREEN_W, SCREEN_H},
                (Rectangle){(float)vx, (float)vy, (float)vw, (float)vh},
                (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
            EndDrawing();
        }
    }

    platform_shutdown();
    return 0;
}
```

---

## Step 4 — Updated X11 `main_x11.c` (Lesson 02 stage)

The render section replaces the plain `memset` with `draw_rect` calls.
The display_backbuffer and platform functions are identical to Lesson 01.

```c
/* main_x11.c — X11 Backend (Lesson 02: drawing primitives) */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "platform.h"
#include "utils/draw-shapes.h"

static Display   *g_display  = NULL;
static Window     g_window   = 0;
static GLXContext  g_gl_ctx  = NULL;
static Atom        g_wm_delete;
static int         g_win_w   = SCREEN_W, g_win_h = SCREEN_H;
static uint32_t    g_pixel_buf[SCREEN_W * SCREEN_H];
static GLuint      g_texture = 0;

int platform_init(PlatformGameProps *props) {
    g_display = XOpenDisplay(NULL);
    if (!g_display) { fprintf(stderr, "Cannot open display\n"); return 0; }
    int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
                      GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, None };
    int screen = DefaultScreen(g_display);
    XVisualInfo *vi = glXChooseVisual(g_display, screen, attribs);
    if (!vi) { fprintf(stderr, "No GLX visual\n"); return 0; }
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(g_display, RootWindow(g_display, vi->screen),
                                   vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;
    g_window = XCreateWindow(g_display, RootWindow(g_display, vi->screen),
        0, 0, (unsigned)props->width, (unsigned)props->height, 0,
        vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(g_display, g_window, props->title);
    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);
    XkbSetDetectableAutoRepeat(g_display, True, NULL);
    XMapWindow(g_display, g_window);
    XFlush(g_display);
    g_gl_ctx = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_display, g_window, g_gl_ctx);
    XFree(vi);
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_W, SCREEN_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_pixel_buf);
    props->backbuffer.pixels = g_pixel_buf;
    props->backbuffer.width  = SCREEN_W;
    props->backbuffer.height = SCREEN_H;
    props->backbuffer.pitch  = SCREEN_W * 4;
    props->is_running = 1;
    return 1;
}

double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void platform_get_input(GameInput *input, PlatformGameProps *props) {
    while (XPending(g_display) > 0) {
        XEvent ev; XNextEvent(g_display, &ev);
        switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == g_wm_delete)
                    { input->quit = 1; props->is_running = 0; }
                break;
            case ConfigureNotify:
                g_win_w = ev.xconfigure.width;
                g_win_h = ev.xconfigure.height;
                break;
            case KeyPress: case KeyRelease: {
                KeySym sym = XkbKeycodeToKeysym(g_display,
                                                ev.xkey.keycode, 0, 0);
                if (sym == XK_Escape || sym == XK_q || sym == XK_Q)
                    { input->quit = 1; props->is_running = 0; }
                break;
            }
        }
    }
}

void platform_shutdown(void) {
    if (g_texture) { glDeleteTextures(1, &g_texture); g_texture = 0; }
    if (g_gl_ctx)  {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_gl_ctx); g_gl_ctx = NULL;
    }
    if (g_window)  { XDestroyWindow(g_display, g_window);  g_window  = 0; }
    if (g_display) { XCloseDisplay(g_display); g_display = NULL; }
}

static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, g_pixel_buf);
    float sx = (float)g_win_w / SCREEN_W, sy = (float)g_win_h / SCREEN_H;
    float sc = (sx < sy) ? sx : sy;
    int vw = (int)(SCREEN_W * sc), vh = (int)(SCREEN_H * sc);
    int vx = (g_win_w - vw) / 2,   vy = (g_win_h - vh) / 2;
    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glViewport(vx, vy, vw, vh);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex2f(-1,-1);
        glTexCoord2f(1,1); glVertex2f( 1,-1);
        glTexCoord2f(1,0); glVertex2f( 1, 1);
        glTexCoord2f(0,0); glVertex2f(-1, 1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glXSwapBuffers(g_display, g_window);
}

int main(void) {
    PlatformGameProps props;
    memset(&props, 0, sizeof(props));
    props.title = "Frogger"; props.width = SCREEN_W; props.height = SCREEN_H;
    if (!platform_init(&props)) return 1;

    GameInput inputs[2]; memset(inputs, 0, sizeof(inputs));

    while (props.is_running) {
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        Backbuffer *bb = &props.backbuffer;

        /* Pure-red validation test: identical result expected on X11 + Raylib */
        draw_rect(bb, 0, 0, bb->width, bb->height, GAME_RGBA(200, 0, 0, 255));
        draw_rect(bb, 20,  20, 120, 80, COLOR_WHITE);
        draw_rect(bb, 160, 20, 120, 80, COLOR_GREEN);
        draw_rect(bb, 300, 20, 120, 80, COLOR_YELLOW);
        draw_rect(bb, 440, 20, 120, 80, COLOR_CYAN);
        draw_rect_blend(bb, bb->width / 2, 0, bb->width / 2, bb->height,
                        COLOR_OVERLAY_DIM);

        display_backbuffer();
    }

    platform_shutdown();
    return 0;
}
```

---

## Step 5 — Updated `build-dev.sh` (Lesson 02)

Add `src/utils/draw-shapes.c` to the source list:

```bash
#!/bin/bash
set -e
mkdir -p build

BACKEND="raylib"; RUN_AFTER_BUILD=false; DEBUG_BUILD=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*) BACKEND="${1#*=}" ;;
        -r|--run)    RUN_AFTER_BUILD=true ;;
        -d|--debug)  DEBUG_BUILD=true ;;
        --help|-h)   echo "Usage: $0 [--backend=x11|raylib] [-r] [-d]"; exit 0 ;;
        *) echo "Unknown: $1" >&2; exit 1 ;;
    esac; shift
done

# Platform-independent sources: add draw-shapes.c here
SOURCES="src/utils/draw-shapes.c src/main_${BACKEND}.c"
LIBS="-lm"
ASSETS_FLAGS="-DASSETS_DIR=\"$(pwd)/assets\""

case "$BACKEND" in
    x11)    LIBS="$LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound" ;;
    raylib) LIBS="$LIBS -lraylib -lpthread -ldl" ;;
    *)      echo "Unknown backend: $BACKEND" >&2; exit 1 ;;
esac

FLAGS="-Wall -Wextra -g -O0 $ASSETS_FLAGS"
[[ "$DEBUG_BUILD" == true ]] && FLAGS="$FLAGS -fsanitize=address,undefined -DDEBUG"

OUTPUT="./build/frogger"
clang $FLAGS -o "$OUTPUT" $SOURCES $LIBS
echo "Build OK: $OUTPUT"
[[ "$RUN_AFTER_BUILD" == true ]] && exec "$OUTPUT"
```

---

## Build and Run

```bash
# Build with Raylib (default)
./build-dev.sh -r

# Build with X11
./build-dev.sh --backend=x11 -r
```

Run both and visually confirm:
1. Deep-red left half with white, green, yellow, cyan rectangles
2. Right half slightly darker (the 63%-opaque black overlay)
3. Both backends look **identical** — same colours, same positions

---

## Expected Result

| Region         | Colour                         |
|----------------|--------------------------------|
| Background     | Deep red `(200, 0, 0)`         |
| Left of centre | Unmodified red + coloured boxes|
| Right of centre| ~63% darkened (overlay at α=160)|
| White rect     | `(255, 255, 255)`              |
| Green rect     | `(0, 255, 0)`                  |
| Yellow rect    | `(255, 255, 0)`                |
| Cyan rect      | `(0, 255, 255)`                |

If the background is **cyan** instead of red on the X11 backend, check that
`glTexImage2D` uses `GL_RGBA` (not `GL_BGRA`).

---

## Exercises

1. Draw a checkerboard pattern using a nested loop and alternating
   `COLOR_WHITE` / `COLOR_DARK_GREY` cells, each 16×16 pixels.
2. Change the overlay alpha to `255` — it should become a fully opaque black
   rectangle covering the right half.  Then set it to `0` — the right half
   should look identical to the left.
3. Write a `draw_pixel(bb, x, y, color)` helper that bounds-checks and writes
   one pixel.  Use it in a loop to draw a diagonal line from `(0,0)` to `(63,63)`.
4. Verify that `GAME_RGBA(255, 0, 0, 255)` gives `0xFF0000FF` in a debugger
   watch window — confirming R is in the low byte.

---

## What's next

**Lesson 03** adds the input system: `GameButtonState`, the `inputs[2]`
double-buffer, and `prepare_input_frame`.  By the end of that lesson, arrow
keys will move a coloured rectangle one tile per press.
