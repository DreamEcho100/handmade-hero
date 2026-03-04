# Lesson 01: Window + Backbuffer

## What we're building

A resizable 1024×640 window that opens, shows a black screen, and exits cleanly
when you press Q, Escape, or click the ×.  No game logic yet — just the
platform scaffolding every subsequent lesson builds on.

The window uses a **software framebuffer** (a flat `uint32_t` array in RAM that
we draw into before sending it to the screen) and a **letterbox algorithm** that
keeps the game canvas at its native aspect ratio as you resize the window.

---

## What you'll learn

- How a CPU pixel backbuffer maps to `canvas.getContext('2d').ImageData` in JS
- Why `pitch` matters and how to use it safely
- `DEBUG_TRAP` and `ASSERT` — C's equivalent of `console.assert` that actually halts
- `PlatformGameProps` — a struct that carries all window/backbuffer state
- `platform_shutdown` — the clean-teardown function the reference source lacked
- The letterbox centering algorithm in both backends
- How `build-dev.sh --backend=x11|raylib -r -d` works

---

## Prerequisites

- C compiler: `clang` or `gcc`
- **X11 backend (Linux):** `sudo apt install libx11-dev libxkbcommon-dev libasound2-dev`
- **Raylib backend:** `sudo apt install libraylib-dev`
- Basic C: `int`, `uint32_t`, `struct`, `#define`, `#ifdef`

---

## Concepts

### 1. The Software Framebuffer (Backbuffer)

In a browser game you call `ctx.fillRect()` on a `<canvas>`.  The browser owns
the pixel memory and uploads it to the GPU for you.  In this course **we own the
pixel memory directly** — an ordinary C array of `uint32_t` values, one per pixel.

```c
// JS: const fb = ctx.createImageData(1024, 640);
// C equivalent:

typedef struct {
    uint32_t *pixels;  /* flat array: width * height elements            */
    int       width;   /* pixels per row  = 1024                        */
    int       height;  /* number of rows  =  640                        */
    int       pitch;   /* bytes per row   = width * 4 = 4096            */
} Backbuffer;
```

`pixels` is just a pointer — we never allocate in `game.c`.  The platform
backend (`main_x11.c` or `main_raylib.c`) owns the actual storage:

```c
// Platform side — one allocation at startup
static uint32_t g_pixel_buf[SCREEN_W * SCREEN_H];   // 1024 * 640 * 4 = 2.5 MB

// Pointer handed to game code
props->backbuffer.pixels = g_pixel_buf;
props->backbuffer.pitch  = SCREEN_W * 4;             // 4096 bytes per row
```

**Why `pitch` and not just `width`?**

Some GPU drivers pad each row to a 64-byte boundary.  If the logical width is
1024 pixels (4096 bytes) that happens to be a multiple of 64, but a width of
100 pixels (400 bytes) would get padded to 448 bytes on those drivers.  By
keeping `pitch` explicit and always computing the pixel index as
`pixels[row * (pitch / 4) + col]` instead of `pixels[row * width + col]`,
the code stays correct even on those drivers.

```c
// Row-major pixel index (correct, robust):
bb->pixels[y * (bb->pitch / 4) + x] = color;

// Fragile version (breaks if GPU pads rows):
bb->pixels[y * bb->width + x] = color;
```

### 2. `GAME_RGBA` — Bit-Packing a Colour

Each pixel is one `uint32_t`.  The bytes in memory on a little-endian CPU are
`[R][G][B][A]` from lowest to highest address.  As a 32-bit integer that is:

```
0xAA BB GG RR
   │  │  │  └─ R = bits 0–7  (least significant)
   │  │  └──── G = bits 8–15
   │  └──────── B = bits 16–23
   └──────────── A = bits 24–31 (most significant)
```

The `GAME_RGBA` macro packs four bytes:

```c
#define GAME_RGBA(r, g, b, a) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | (uint32_t)(r))
```

```c
// JS equivalent (little-endian DataView):
// function rgba(r, g, b, a) {
//   const buf = new Uint8Array([r, g, b, a]);
//   return new DataView(buf.buffer).getUint32(0, true); // little-endian
// }

uint32_t black = GAME_RGBA(  0,   0,   0, 255);  /* = 0xFF000000 */
uint32_t white = GAME_RGBA(255, 255, 255, 255);  /* = 0xFFFFFFFF */
uint32_t red   = GAME_RGBA(255,   0,   0, 255);  /* = 0xFF0000FF */
```

**OpenGL consequence:**  When uploading with `glTexImage2D(…, GL_RGBA, GL_UNSIGNED_BYTE, pixels)`,
OpenGL reads bytes in memory as R, G, B, A — exactly matching our layout.
Do **not** use `GL_BGRA` here; that swaps red and blue, making all sprites the
wrong colour.

### 3. `DEBUG_TRAP` and `ASSERT`

JavaScript's `console.assert` is non-fatal — execution continues.
C's `DEBUG_TRAP` triggers a CPU breakpoint so the debugger catches the exact line:

```c
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()     /* GCC/Clang: stop here in gdb */
  #define ASSERT(cond, msg) \
      do { \
          if (!(cond)) { \
              fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d\n", \
                      (msg), __FILE__, __LINE__); \
              DEBUG_TRAP(); \
          } \
      } while (0)
#else
  #define DEBUG_TRAP()          ((void)0)    /* compiled out in release */
  #define ASSERT(cond, msg)     ((void)0)
#endif
```

```c
// JS equivalent:
// console.assert(ptr !== null, "ptr must not be null");
// // ↑ logs warning; JS execution continues

// C equivalent (halts and breaks into debugger if ptr == NULL):
ASSERT(ptr != NULL, "ptr must not be null");
```

Enable it with `./build-dev.sh -d` (`-DDEBUG` + AddressSanitizer).

### 4. `PlatformGameProps` — Centralised Window State

> **Course Note:** The reference Frogger source passes `title`, `width`, and
> `height` as separate arguments to `platform_init`.  The course wraps them in
> `PlatformGameProps`, matching the convention established in the
> Tetris/Snake/Asteroids courses.  Benefits: (a) adding a new field (e.g.,
> `is_fullscreen`) only changes the struct, not every function signature;
> (b) `is_running` lives alongside the other window state rather than as a
> separate global.

```c
typedef struct {
    const char *title;
    int         width;
    int         height;
    Backbuffer  backbuffer;
    int         is_running;  /* set to 0 by platform when window is closed */
} PlatformGameProps;
```

```c
// JS analogy:
// class PlatformProps {
//   title = "Frogger";
//   width = 1024;
//   height = 640;
//   isRunning = true;
//   backbuffer = new ImageData(1024, 640);
// }
```

### 5. `platform_shutdown` — Clean Teardown

> **Course Note:** The reference source has no `platform_shutdown`.  Without it,
> clicking the window's × button leaves the ALSA audio device locked and the
> terminal process in a zombie state — you have to kill the terminal.
> The course adds `platform_shutdown()` for clean resource teardown, matching
> the Tetris/Snake/Asteroids pattern.

```c
// Called once at the end of main(), after the game loop exits.
void platform_shutdown(void);

// X11 implementation: drain ALSA → close GL context → destroy window → close display
// Raylib implementation: StopAudioStream → CloseAudioDevice → UnloadTexture → CloseWindow
```

### 6. The Letterbox Algorithm

When the window is resized, the game canvas scales uniformly (preserving aspect
ratio) and centres with black bars filling the remainder:

```
scale_x = window_width  / SCREEN_W     ← how much to scale to fill width
scale_y = window_height / SCREEN_H     ← how much to scale to fill height
scale   = min(scale_x, scale_y)        ← use whichever axis is tighter
viewport_w = SCREEN_W * scale
viewport_h = SCREEN_H * scale
viewport_x = (window_width  - viewport_w) / 2   ← centre horizontally
viewport_y = (window_height - viewport_h) / 2   ← centre vertically
```

In X11:
```c
// 1. Clear entire window (fills letterbox bars with black)
glViewport(0, 0, g_win_w, g_win_h);
glClear(GL_COLOR_BUFFER_BIT);

// 2. Draw game canvas in the computed sub-rectangle
glViewport(vx, vy, vw, vh);
// … draw textured quad covering [-1,+1]×[-1,+1] …
glXSwapBuffers(g_display, g_window);
```

In Raylib:
```c
ClearBackground(BLACK);              // fills letterbox bars
DrawTexturePro(g_texture,
    (Rectangle){0, 0, SCREEN_W, SCREEN_H},          // source = full canvas
    (Rectangle){vx, vy, vw, vh},                    // dest = centred viewport
    (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
```

### 7. `build-dev.sh` — Unified Build Script

> **Course Note:** The reference Frogger source ships two separate build scripts:
> `build_x11.sh` and `build_raylib.sh`.  The course replaces both with a single
> `build-dev.sh --backend=x11|raylib`, matching the Tetris/Asteroids workflow.
> The trade-off: slightly more shell argument parsing, but compiler flags and
> source lists are maintained in one place.  See `COURSE-BUILDER-IMPROVEMENTS.md`
> for the full rationale.

```bash
./build-dev.sh                      # default: Raylib backend
./build-dev.sh --backend=x11        # X11 / GLX / ALSA (Linux only)
./build-dev.sh -r                   # build AND run
./build-dev.sh -d                   # AddressSanitizer + -DDEBUG
./build-dev.sh --backend=x11 -r -d  # all three combined
```

---

## Step 1 — `src/utils/backbuffer.h`

This file defines the `Backbuffer` struct, `GAME_RGBA`, screen dimensions, and
the `DEBUG_TRAP`/`ASSERT` macros.  It is included by both platform and game code.

```c
/* utils/backbuffer.h — Pixel Backbuffer + Color Encoding */
#ifndef FROGGER_BACKBUFFER_H
#define FROGGER_BACKBUFFER_H

#include <stdint.h>   /* uint32_t, uint8_t */
#include <stdio.h>    /* fprintf (ASSERT) */

/* ── Screen Dimensions ──────────────────────────────────────────────────── */
/* 128 cells × 80 cells, each cell = 8 pixels → 1024 × 640 px             */
#define SCREEN_CELLS_W  128
#define SCREEN_CELLS_H   80
#define CELL_PX           8
#define SCREEN_W  (SCREEN_CELLS_W * CELL_PX)   /* 1024 pixels */
#define SCREEN_H  (SCREEN_CELLS_H * CELL_PX)   /*  640 pixels */

/* ── Backbuffer ──────────────────────────────────────────────────────────
   Platform code allocates the pixel array; game code only writes into it.
   Passed by pointer to game_render() so the game never sees the platform.  */
typedef struct {
    uint32_t *pixels;  /* flat array: row-major, width * height elements  */
    int       width;   /* = SCREEN_W after init                           */
    int       height;  /* = SCREEN_H after init                           */
    int       pitch;   /* bytes per row = width * 4                       */
} Backbuffer;

/* ── GAME_RGBA ────────────────────────────────────────────────────────────
   Memory layout on little-endian: [R byte][G byte][B byte][A byte]
   As uint32_t: 0xAABBGGRR  (R = low byte, A = high byte)

   JS equivalent:
     const buf = new Uint8Array([r, g, b, a]);
     return new DataView(buf.buffer).getUint32(0, true);                  */
#define GAME_RGBA(r, g, b, a) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(g) << 8 | (uint32_t)(r))

#define GAME_RGB(r, g, b)  GAME_RGBA(r, g, b, 255)

/* ── Named Colour Constants ──────────────────────────────────────────── */
#define COLOR_BLACK        GAME_RGBA(  0,   0,   0, 255)
#define COLOR_WHITE        GAME_RGBA(255, 255, 255, 255)
#define COLOR_RED          GAME_RGBA(255,   0,   0, 255)
#define COLOR_GREEN        GAME_RGBA(  0, 255,   0, 255)
#define COLOR_BLUE         GAME_RGBA(  0,   0, 255, 255)
#define COLOR_YELLOW       GAME_RGBA(255, 255,   0, 255)
#define COLOR_CYAN         GAME_RGBA(  0, 255, 255, 255)
#define COLOR_ORANGE       GAME_RGBA(255, 165,   0, 255)
#define COLOR_GREY         GAME_RGBA(128, 128, 128, 255)
#define COLOR_DARK_GREY    GAME_RGBA( 40,  40,  40, 255)
#define COLOR_OVERLAY_DIM  GAME_RGBA(  0,   0,   0, 160)

/* ── Debug Utilities ─────────────────────────────────────────────────── */
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()
  #define ASSERT(cond, msg) \
      do { \
          if (!(cond)) { \
              fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d\n", \
                      (msg), __FILE__, __LINE__); \
              DEBUG_TRAP(); \
          } \
      } while (0)
#else
  #define DEBUG_TRAP()          ((void)0)
  #define ASSERT(cond, msg)     ((void)0)
#endif

#endif /* FROGGER_BACKBUFFER_H */
```

---

## Step 2 — `src/platform.h`

The platform contract: four functions every backend must implement.

```c
/* platform.h — Platform API Contract */
#ifndef FROGGER_PLATFORM_H
#define FROGGER_PLATFORM_H

#include "utils/backbuffer.h"

/* ── Input stubs (filled in Lesson 03) ─────────────────────────────────── */
typedef struct { int ended_down; int half_transition_count; } GameButtonState;
typedef struct {
    union {
        GameButtonState buttons[4];
        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;
        };
    };
    int quit;
    int restart;
} GameInput;

/* ── PlatformGameProps ──────────────────────────────────────────────────
   Course Note: reference uses raw platform_init(title, w, h).
   We wrap in a struct so adding fields (fullscreen, audio ptr) never
   changes function signatures.                                           */
typedef struct {
    const char *title;
    int         width;
    int         height;
    Backbuffer  backbuffer;
    int         is_running;
} PlatformGameProps;

/* ── Four-function Platform Contract ─────────────────────────────────── */

/* Open window + GL context + audio device.  Returns 1 on success.       */
int  platform_init(PlatformGameProps *props);

/* Monotonic clock in seconds.  Used to compute delta_time each frame.   */
double platform_get_time(void);

/* Read keyboard events into *input.  Sets props->is_running=0 on quit.  */
void platform_get_input(GameInput *input, PlatformGameProps *props);

/* Release all platform resources.  Call once after the game loop exits.
   Course Note: absent from the reference source; added to prevent ALSA
   device lock and zombie terminal process on exit.                       */
void platform_shutdown(void);

/* Swap two GameInput buffers in-place.  Used for the double-buffer      */
static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    GameInput tmp = *a; *a = *b; *b = tmp;
}

#endif /* FROGGER_PLATFORM_H */
```

---

## Step 3 — Raylib backend: `src/main_raylib.c` (Lesson 01 stage)

At this stage the Raylib backend opens a resizable window and clears it black
each frame.  No game logic, no input yet.

```c
/* main_raylib.c — Raylib Platform Backend (Lesson 01: window only) */
#include <string.h>
#include <stdint.h>
#include "raylib.h"
#include "platform.h"

/* ── Global Platform State ─────────────────────────────────────────────── */
static Texture2D g_texture;
static uint32_t  g_pixel_buf[SCREEN_W * SCREEN_H];

/* ── platform_init ─────────────────────────────────────────────────────── */
int platform_init(PlatformGameProps *props) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(props->width, props->height, props->title);
    SetTargetFPS(60);

    /* CPU image → GPU texture.
       PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bytes in memory = [R][G][B][A]
       — matches our GAME_RGBA layout exactly.                            */
    Image img = {
        .data    = g_pixel_buf,
        .width   = SCREEN_W,
        .height  = SCREEN_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT); /* crisp pixels  */

    props->backbuffer.pixels = g_pixel_buf;
    props->backbuffer.width  = SCREEN_W;
    props->backbuffer.height = SCREEN_H;
    props->backbuffer.pitch  = SCREEN_W * 4;
    props->is_running        = 1;
    return 1;
}

/* ── platform_get_time ─────────────────────────────────────────────────── */
double platform_get_time(void) { return GetTime(); }

/* ── platform_get_input ────────────────────────────────────────────────── */
void platform_get_input(GameInput *input, PlatformGameProps *props) {
    if (WindowShouldClose() ||
        IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
        input->quit       = 1;
        props->is_running = 0;
    }
}

/* ── platform_shutdown ─────────────────────────────────────────────────── */
void platform_shutdown(void) {
    UnloadTexture(g_texture);
    CloseWindow();
}

/* ── main ──────────────────────────────────────────────────────────────── */
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
        /* ── Input ─────────────────────────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        /* ── "Render": clear to black ───────────────────────────────── */
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
            g_pixel_buf[i] = COLOR_BLACK;

        /* ── Display: letterbox scale + blit ───────────────────────── */
        UpdateTexture(g_texture, g_pixel_buf);
        {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float sx = (float)sw / (float)SCREEN_W;
            float sy = (float)sh / (float)SCREEN_H;
            float sc = (sx < sy) ? sx : sy;
            int   vw = (int)((float)SCREEN_W * sc);
            int   vh = (int)((float)SCREEN_H * sc);
            int   vx = (sw - vw) / 2;
            int   vy = (sh - vh) / 2;

            BeginDrawing();
                ClearBackground(BLACK);
                DrawTexturePro(g_texture,
                    (Rectangle){0, 0, (float)SCREEN_W, (float)SCREEN_H},
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

## Step 4 — X11 backend: `src/main_x11.c` (Lesson 01 stage)

The X11 backend does the same job with raw Xlib + GLX.  More lines, same
behaviour — same black window, same letterbox.

```c
/* main_x11.c — X11/GLX Platform Backend (Lesson 01: window only) */
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

/* ── Global Platform State ─────────────────────────────────────────────── */
static Display   *g_display   = NULL;
static Window     g_window    = 0;
static GLXContext  g_gl_ctx   = NULL;
static Atom        g_wm_delete = 0;
static int         g_win_w    = SCREEN_W;
static int         g_win_h    = SCREEN_H;

static uint32_t    g_pixel_buf[SCREEN_W * SCREEN_H];
static GLuint      g_texture  = 0;

/* ── platform_init ─────────────────────────────────────────────────────── */
int platform_init(PlatformGameProps *props) {
    g_display = XOpenDisplay(NULL);
    if (!g_display) { fprintf(stderr, "Cannot open X11 display\n"); return 0; }

    int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
                      GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                      GLX_BLUE_SIZE, 8, None };
    int screen = DefaultScreen(g_display);
    XVisualInfo *vi = glXChooseVisual(g_display, screen, attribs);
    if (!vi) { fprintf(stderr, "No suitable GLX visual\n"); return 0; }

    XSetWindowAttributes swa;
    swa.colormap   = XCreateColormap(g_display,
                         RootWindow(g_display, vi->screen), vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;
    g_window = XCreateWindow(
        g_display, RootWindow(g_display, vi->screen),
        0, 0, (unsigned)props->width, (unsigned)props->height,
        0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWEventMask, &swa);

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
    props->is_running        = 1;
    return 1;
}

/* ── platform_get_time ─────────────────────────────────────────────────── */
double platform_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ── platform_get_input ────────────────────────────────────────────────── */
void platform_get_input(GameInput *input, PlatformGameProps *props) {
    while (XPending(g_display) > 0) {
        XEvent ev;
        XNextEvent(g_display, &ev);
        switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == g_wm_delete) {
                    input->quit = 1; props->is_running = 0;
                }
                break;
            case ConfigureNotify:
                g_win_w = ev.xconfigure.width;
                g_win_h = ev.xconfigure.height;
                break;
            case KeyPress: case KeyRelease: {
                KeySym sym = XkbKeycodeToKeysym(g_display,
                                                ev.xkey.keycode, 0, 0);
                if (sym == XK_Escape || sym == XK_q || sym == XK_Q) {
                    input->quit = 1; props->is_running = 0;
                }
                break;
            }
        }
    }
}

/* ── platform_shutdown ─────────────────────────────────────────────────── */
void platform_shutdown(void) {
    if (g_texture)  { glDeleteTextures(1, &g_texture); g_texture = 0; }
    if (g_gl_ctx)   {
        glXMakeCurrent(g_display, None, NULL);
        glXDestroyContext(g_display, g_gl_ctx); g_gl_ctx = NULL;
    }
    if (g_window)   { XDestroyWindow(g_display, g_window);  g_window  = 0; }
    if (g_display)  { XCloseDisplay(g_display); g_display = NULL; }
}

/* ── display_backbuffer — upload + letterbox blit ───────────────────────── */
static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, g_pixel_buf);

    float scale_x = (float)g_win_w / (float)SCREEN_W;
    float scale_y = (float)g_win_h / (float)SCREEN_H;
    float scale   = (scale_x < scale_y) ? scale_x : scale_y;
    int   vw = (int)((float)SCREEN_W * scale);
    int   vh = (int)((float)SCREEN_H * scale);
    int   vx = (g_win_w - vw) / 2;
    int   vy = (g_win_h - vh) / 2;

    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(vx, vy, vw, vh);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glXSwapBuffers(g_display, g_window);
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void) {
    PlatformGameProps props;
    memset(&props, 0, sizeof(props));
    props.title  = "Frogger";
    props.width  = SCREEN_W;
    props.height = SCREEN_H;

    if (!platform_init(&props)) return 1;

    GameInput inputs[2];
    memset(inputs, 0, sizeof(inputs));

    double t_last = platform_get_time();

    while (props.is_running) {
        double t_now = platform_get_time();
        double dt    = t_now - t_last;
        t_last       = t_now;
        if (dt > 0.066) dt = 0.066;
        (void)dt;  /* not used yet */

        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        /* Clear to black */
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
            g_pixel_buf[i] = COLOR_BLACK;

        display_backbuffer();
    }

    platform_shutdown();
    return 0;
}
```

---

## Step 5 — `build-dev.sh`

```bash
#!/bin/bash
# build-dev.sh — Unified Development Build Script

set -e
mkdir -p build

BACKEND="raylib"
RUN_AFTER_BUILD=false
DEBUG_BUILD=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*) BACKEND="${1#*=}" ;;
        -r|--run)    RUN_AFTER_BUILD=true ;;
        -d|--debug)  DEBUG_BUILD=true ;;
        --help|-h)
            echo "Usage: $0 [--backend=x11|raylib] [-r] [-d]"
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

SOURCES="src/main_${BACKEND}.c"
LIBS="-lm"
ASSETS_FLAGS="-DASSETS_DIR=\"$(pwd)/assets\""

case "$BACKEND" in
    x11)    LIBS="$LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound" ;;
    raylib) LIBS="$LIBS -lraylib -lpthread -ldl" ;;
    *)      echo "Unknown backend: $BACKEND" >&2; exit 1 ;;
esac

FLAGS="-Wall -Wextra -g -O0 $ASSETS_FLAGS"
if [[ "$DEBUG_BUILD" == true ]]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -DDEBUG"
fi

OUTPUT="./build/frogger"
clang $FLAGS -o "$OUTPUT" $SOURCES $LIBS
echo "Build successful: $OUTPUT"

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    exec "$OUTPUT"
fi
```

> **Note:** Once we add `game.c` and utility files in later lessons, the
> `SOURCES` line will expand to include them.  The structure stays the same.

---

## Build and Run

Install dependencies first (once):

```bash
# Raylib
sudo apt install libraylib-dev

# X11
sudo apt install libx11-dev libxkbcommon-dev libasound2-dev libgl1-mesa-dev

chmod +x build-dev.sh
```

Build and run:

```bash
# Raylib (default)
./build-dev.sh -r

# X11
./build-dev.sh --backend=x11 -r

# Debug build (AddressSanitizer + -DDEBUG)
./build-dev.sh -d -r
```

---

## Expected Result

A **1024×640 black window** appears.  Resize it freely — the black canvas
scales uniformly and stays centred with black letterbox bars.  Press Q, Escape,
or the window × button to exit cleanly.  The terminal should return to your
prompt immediately (no zombie process) thanks to `platform_shutdown`.

---

## Exercises

1. Change the clear colour to `COLOR_DARK_GREY` and confirm both backends show
   the same shade.
2. Resize the window to portrait orientation (taller than wide) — verify the
   letterbox bars appear on the left and right instead of top and bottom.
3. Add `ASSERT(props->backbuffer.pixels != NULL, "backbuffer not allocated")`
   after `platform_init` and trigger it by commenting out the `pixels = g_pixel_buf`
   line — run with `-d` to see the assert fire in the debugger.
4. Try `./build-dev.sh --backend=bogus` — confirm the script prints a helpful error
   and exits with code 1.

---

## What's next

In **Lesson 02** we write `draw_rect` and `draw_rect_blend` — the primitives
that every subsequent lesson uses — and perform the pure-red cross-backend
validation test to confirm `GAME_RGBA` byte ordering is correct on both X11
and Raylib.
