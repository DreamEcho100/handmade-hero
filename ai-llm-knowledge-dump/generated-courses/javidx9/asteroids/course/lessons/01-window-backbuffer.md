# Lesson 01 — Window + Backbuffer

## What you'll build
A **512×480 pixel window** that opens, renders nothing (black), and exits
cleanly when you press Q, Escape, or click the window's × button.
No game logic yet — just the platform scaffolding every subsequent lesson
builds on.

---

## Prerequisites
- C compiler (`clang` or `gcc`)
- For X11 backend: `libx11-dev libxkbcommon-dev libasound2-dev`  
- For Raylib backend: `libraylib-dev`
- A basic grasp of C types (`int`, `float`, `char *`)

---

## Core Concepts

### 1. The Software Framebuffer (Backbuffer)

In a browser game you draw by calling `ctx.fillRect(...)` or updating an
`ImageData` array.  In this C course we own the pixel memory directly.

```c
// JS equivalent:
// const fb = document.createElement('canvas');
// const ctx = fb.getContext('2d');
// const imageData = ctx.createImageData(512, 480);

typedef struct {
    uint32_t *pixels;  // flat array of width*height pixels
    int       width;   // 512
    int       height;  // 480
    int       pitch;   // bytes per row = width * 4
} AsteroidsBackbuffer;
```

**Why `pitch`?**  
`pitch` is the byte-width of one row.  It equals `width * 4` for us, but
some GPU drivers pad rows to 64-byte boundaries.  By always using
`pixels[y * (pitch/4) + x]` instead of `pixels[y * width + x]` the code
works correctly even on those drivers.

### 2. GAME_RGBA — Pixel Colour Encoding

Each pixel is one `uint32_t`.  The byte layout in memory is `[R][G][B][A]`
(little-endian, lowest address = R).  As a 32-bit value: `0xAABBGGRR`.

```c
// Pack four bytes into one uint32_t:
#define GAME_RGBA(r, g, b, a) \
    ((uint32_t)(a)<<24 | (uint32_t)(b)<<16 | (uint32_t)(g)<<8 | (uint32_t)(r))

// Example: fully opaque white
uint32_t white = GAME_RGBA(255, 255, 255, 255);  // = 0xFFFFFFFF
```

**Why R in the low byte?**  
OpenGL reads `GL_RGBA` as `[R][G][B][A]` in memory order, which is our
layout exactly.  Using `GL_BGRA` would swap red and blue — causing cyan
objects to appear orange and vice versa.  This bit tripped up the Snake
course; we document it here explicitly.

### 3. `DEBUG_TRAP` and `ASSERT`

```c
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()  // fires CPU breakpoint
  #define ASSERT(cond, msg) \
      do { if (!(cond)) { fprintf(stderr, "ASSERT: %s at %s:%d\n", \
           msg, __FILE__, __LINE__); DEBUG_TRAP(); } } while(0)
#else
  #define DEBUG_TRAP()        ((void)0)  // compiled out in release
  #define ASSERT(cond, msg)   ((void)0)
#endif
```

JS equivalent: `console.assert(cond, msg)` — but JS never halts the program.
C's `DEBUG_TRAP` lets the debugger catch the exact line that failed.

### 4. The Platform Contract

`platform.h` declares four functions that the platform (OS) provides and
the game calls, plus four functions that the game provides and the platform
calls.  Right now only the platform side matters:

```c
void platform_init(void);      // open window, create GL context
void platform_shutdown(void);  // destroy GL context, close window
```

`platform_shutdown` is the lesson-one addition the reference C source lacked.
Without it, closing the window left the terminal process hung.

### 5. `build-dev.sh` — Unified Build Script

```bash
./build-dev.sh                  # Raylib (default)
./build-dev.sh --backend=x11    # X11/GLX/ALSA
./build-dev.sh -r               # build and run
```

The reference source had two separate scripts (`build_x11.sh`,
`build_raylib.sh`) with duplicated flags.  The unified script is shorter
and ensures both backends stay in sync.

---

## Implement It

### Raylib backend

Create this minimal `main_raylib.c` for lesson 1 (no game logic yet):

```c
#include "raylib.h"
#include "platform.h"

static uint32_t  g_pixel_buf[SCREEN_W * SCREEN_H];
static Texture2D g_texture;

void platform_init(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Asteroids");
    SetWindowState(FLAG_WINDOW_RESIZABLE);  // allow resize; canvas is letterboxed
    SetTargetFPS(60);
    Image img = { .data = g_pixel_buf, .width = SCREEN_W,
                  .height = SCREEN_H, .mipmaps = 1,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);
}

void platform_shutdown(void) {
    UnloadTexture(g_texture);
    CloseWindow();
}

int main(void) {
    platform_init();

    // Fill all pixels with black
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        g_pixel_buf[i] = GAME_RGBA(0, 0, 0, 255);

    AsteroidsBackbuffer bb = {
        .pixels = g_pixel_buf, .width = SCREEN_W,
        .height = SCREEN_H, .pitch = SCREEN_W * 4
    };

    while (!WindowShouldClose() && !IsKeyPressed(KEY_Q)
           && !IsKeyPressed(KEY_ESCAPE)) {
        // Upload pixel buffer to GPU texture
        UpdateTexture(g_texture, g_pixel_buf);

        // Compute centred letterbox destination rectangle
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float sc = ((float)sw/SCREEN_W < (float)sh/SCREEN_H)
                   ? (float)sw/SCREEN_W : (float)sh/SCREEN_H;
        int vw = (int)(SCREEN_W*sc), vh = (int)(SCREEN_H*sc);

        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(g_texture,
                (Rectangle){0, 0, SCREEN_W, SCREEN_H},
                (Rectangle){(sw-vw)/2.0f, (sh-vh)/2.0f, vw, vh},
                (Vector2){0,0}, 0.0f, WHITE);
        EndDrawing();
    }

    platform_shutdown();
    return 0;
}
```

### X11 backend

The X11 backend does the same job with raw Xlib + GLX instead of Raylib:

```c
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "platform.h"

static Display   *g_display;
static Window     g_window;
static GLXContext g_gl_ctx;
static Atom       g_wm_delete;
static int        g_win_w = SCREEN_W, g_win_h = SCREEN_H;
static uint32_t   g_pixel_buf[SCREEN_W * SCREEN_H];
static GLuint     g_texture;

void platform_init(void) {
    g_display = XOpenDisplay(NULL);
    int screen = DefaultScreen(g_display);

    // Request an RGBA visual with double-buffering
    static int attr[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
                          GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                          GLX_BLUE_SIZE, 8, None };
    XVisualInfo *vi = glXChooseVisual(g_display, screen, attr);

    // Create window with keyboard + resize events
    Colormap cmap = XCreateColormap(g_display, RootWindow(g_display, vi->screen),
                                    vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap   = cmap;
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;

    g_window = XCreateWindow(g_display, RootWindow(g_display, vi->screen),
        100, 100, SCREEN_W, SCREEN_H, 0, vi->depth, InputOutput,
        vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(g_display, g_window, "Asteroids");
    XMapWindow(g_display, g_window);

    // WM_DELETE_WINDOW: receive a ClientMessage instead of being killed
    g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);

    // Suppress X11 fake key-repeat events
    XkbSetDetectableAutoRepeat(g_display, True, NULL);

    // Create GL context and upload texture storage
    g_gl_ctx = glXCreateContext(g_display, vi, NULL, GL_TRUE);
    glXMakeCurrent(g_display, g_window, g_gl_ctx);
    XFree(vi);

    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Allocate texture storage once; pixels are updated each frame
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_W, SCREEN_H, 0,
                 GL_RGBA,        // MUST be GL_RGBA — NOT GL_BGRA
                 GL_UNSIGNED_BYTE, NULL);
}
```

**X11 pixel upload + centred display:**

```c
static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, g_pixel_buf);

    // Uniform scale: fit SCREEN_W×SCREEN_H into current window, centred
    float sx = (float)g_win_w / SCREEN_W;
    float sy = (float)g_win_h / SCREEN_H;
    float sc = (sx < sy) ? sx : sy;
    int vw = (int)(SCREEN_W * sc), vh = (int)(SCREEN_H * sc);
    int vx = (g_win_w - vw) / 2,   vy = (g_win_h - vh) / 2;

    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);           // paint letterbox bars black

    glViewport(vx, vy, vw, vh);            // clip to game area
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(1, 1); glVertex2f( 1, -1);
        glTexCoord2f(1, 0); glVertex2f( 1,  1);
        glTexCoord2f(0, 0); glVertex2f(-1,  1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glXSwapBuffers(g_display, g_window);
}
```

Window resize is handled by catching `ConfigureNotify` events (which arrive
because `StructureNotifyMask` was set):

```c
case ConfigureNotify:
    g_win_w = event.xconfigure.width;
    g_win_h = event.xconfigure.height;
    break;
```

---

## Verify

```
./build-dev.sh --backend=raylib -r
./build-dev.sh --backend=x11 -r
```

- Raylib: black 512×480 window, Q/Esc closes.
- X11: same window opens; drag the corner to resize → game canvas stays
  centred with black bars filling the extra space.

---

## Platform Comparison Table

| Feature | Raylib | X11 |
|---------|--------|-----|
| Window open | `InitWindow(W,H,title)` | `XCreateWindow` + `XMapWindow` |
| Resizable | `SetWindowState(FLAG_WINDOW_RESIZABLE)` | `StructureNotifyMask` + `ConfigureNotify` |
| Pixel upload | `UpdateTexture(tex, pixels)` | `glTexSubImage2D` |
| Centred draw | `DrawTexturePro(tex, src, dst, …)` | `glViewport(vx,vy,vw,vh)` |
| Clean exit | `WindowShouldClose()` | `WM_DELETE_WINDOW` protocol |
| Lines of code | ~25 for init+draw | ~70 for init+draw |

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Flat pixel array | `uint32_t pixels[W*H]` | `Uint32Array` from `ImageData` |
| Colour packing | `GAME_RGBA(r,g,b,a)` | `(a<<24)\|(b<<16)\|(g<<8)\|r` |
| `pitch` | bytes per row | `imageData.data.byteLength / height` |
| Assertions | `ASSERT(cond, msg)` | `console.assert(cond, msg)` |
| Letterbox scaling | `scale = min(win_w/W, win_h/H)` | CSS `object-fit: contain` |
| Build system | `build-dev.sh` | `package.json` scripts |
