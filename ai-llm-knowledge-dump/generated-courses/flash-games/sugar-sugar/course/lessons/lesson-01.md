# Lesson 01: Window, Canvas, and the Game Loop

## What we're building

A window that opens, fills with a solid sky-blue color every frame, and closes cleanly
when you press Escape or the window's × button.  Nothing simulates yet — this is pure
proof-of-life.  By the end you will have the complete skeleton that all future lessons
build upon, running on **both** Raylib and X11.

## What you'll learn

- How a bare-bones game loop works in C (compare with `requestAnimationFrame` in JS)
- The **backbuffer pattern**: write pixels to a plain array, then let the platform blit it to the screen
- `platform.h` — a thin contract that hides OS differences from game code
- How `platform_init`, `platform_get_time`, and `platform_display_backbuffer` are implemented on Raylib and X11
- How to build and run with a single `clang` command

## Prerequisites

- C99 basics (structs, pointers, `#include`, `typedef`)
- Raylib installed (`brew install raylib` / `sudo apt install libraylib-dev`)
- Optional: X11 (`sudo apt install libx11-dev`)

---

## Step 1: The platform contract

Before writing any game code, decide what the platform must provide.  Put that
contract in `src/platform.h`:

```c
/* src/platform.h  —  Sugar, Sugar | Platform Contract */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "game.h"

/* Open a window with the given title and pixel dimensions. */
void   platform_init(const char *title, int width, int height);

/* Return monotonic time in seconds (never jumps backward). */
double platform_get_time(void);

/* Read OS input events and fill *input with the current state. */
void   platform_get_input(GameInput *input);

/* Upload bb->pixels (ARGB uint32_t) to the display and flip. */
void   platform_display_backbuffer(const GameBackbuffer *backbuffer);

/* Audio — stubbed out for now, added in Lesson 15. */
void   platform_audio_init(GameState *state, int samples_per_second);
void   platform_audio_update(GameState *state);
void   platform_audio_shutdown(void);

#endif /* PLATFORM_H */
```

**What's happening:**

- Any future backend (SDL3, Win32, WASM) only needs to implement these six functions — `game.c` never changes.
- `GameBackbuffer`, `GameInput`, and `GameState` are defined in `game.h` (written next).
- The audio stubs keep the contract complete from day one; the implementations arrive in Lesson 15.

JS analogy: `platform.h` is like a TypeScript interface — the platform backends are the concrete classes that `implements` it.

---

## Step 2: Minimal `game.h`

For this first lesson `game.h` only needs three types:

```c
/* src/game.h  —  Sugar, Sugar | Shared Game Types (Lesson 01 version) */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>   /* uint32_t */

/* Canvas dimensions — fixed 640×480 logical resolution. */
#define CANVAS_W 640
#define CANVAS_H 480

/* -----------------------------------------------------------------------
 * BACKBUFFER
 * A flat array of ARGB pixels.  The game writes here; the platform reads it.
 * JS analogy: this is the ImageData.data buffer behind a 2D <canvas>.
 * ----------------------------------------------------------------------- */
typedef struct {
  uint32_t *pixels; /* pixels[y * width + x] = 0xAARRGGBB color */
  int width;
  int height;
  int pitch;        /* bytes per row = width * 4 */
} GameBackbuffer;

/* Minimal stubs so platform.h compiles — these grow in later lessons. */
typedef struct { int placeholder; } GameInput;
typedef struct { int should_quit; } GameState;

/* Color macro — 0xAARRGGBB format */
#define GAME_RGB(r, g, b) \
  (((uint32_t)0xFF << 24) | ((uint32_t)(r) << 16) | \
   ((uint32_t)(g) << 8)  |  (uint32_t)(b))

#define COLOR_BG  GAME_RGB(135, 195, 225)  /* soft sky-blue */

#endif /* GAME_H */
```

**What's happening:**

- `pixels[y * width + x]` — a single flat array.  Row `y` starts at index `y * width`.
  There is no 2-D array; you compute the index yourself.
- `0xAARRGGBB` — Alpha in the highest byte, Blue in the lowest.  X11 uses BGR internally
  (alpha ignored), so the bytes land in exactly the right places with no extra swap.
- `pitch` = bytes per row = `width × 4`.  It exists for completeness; for a packed
  (no padding) buffer it always equals `width * 4`.

---

## Step 3: Raylib backend

```c
/* src/main_raylib.c  —  Sugar, Sugar | Raylib Platform Backend (Lesson 01) */
#include "raylib.h"
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset       */
#include "platform.h"

/* Texture lives in GPU memory; we update it each frame from the pixel array. */
static Texture2D g_texture;

/* Letterbox scale and offset — needed in Lesson 03 for mouse transforms.
 * Kept as globals here so they are already in place. */
static float g_scale    = 1.0f;
static int   g_offset_x = 0;
static int   g_offset_y = 0;

void platform_init(const char *title, int width, int height) {
    SetTraceLogLevel(LOG_WARNING);      /* suppress Raylib log spam   */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);

    /* Create a texture the same size as the canvas.
     * PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 = 4 bytes per pixel (RGBA order).
     * We swap R↔B before each upload so our ARGB pixels hit the right channels. */
    Image img = {
        .data    = NULL,
        .width   = width,
        .height  = height,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    g_texture = LoadTextureFromImage(img);
    SetTargetFPS(60);
}

double platform_get_time(void) {
    return GetTime(); /* Raylib monotonic clock, returns seconds */
}

void platform_get_input(GameInput *input) {
    (void)input; /* stubbed — added in Lesson 03 */
}

void platform_display_backbuffer(const GameBackbuffer *backbuffer) {
    /* Pixel format fix: our pixels are 0xAARRGGBB stored little-endian as
     * [B, G, R, A].  Raylib RGBA reads those same bytes as [R=B, G=G, B=R, A=A]
     * — red and blue swap.  We fix this with an in-place swap before upload
     * and a matching swap-back so game state is unaffected. */
    uint32_t *px    = backbuffer->pixels;
    int        total = backbuffer->width * backbuffer->height;
    for (int i = 0; i < total; i++) {
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }
    UpdateTexture(g_texture, backbuffer->pixels);
    for (int i = 0; i < total; i++) {     /* swap back */
        uint32_t c = px[i];
        px[i] = (c & 0xFF00FF00u)
              | ((c & 0x00FF0000u) >> 16)
              | ((c & 0x000000FFu) << 16);
    }

    /* Letterbox: scale canvas to fit window with black bars. */
    int   win_w  = GetScreenWidth();
    int   win_h  = GetScreenHeight();
    float scaleX = (float)win_w / (float)CANVAS_W;
    float scaleY = (float)win_h / (float)CANVAS_H;
    g_scale    = (scaleX < scaleY) ? scaleX : scaleY;
    int dst_w  = (int)((float)CANVAS_W * g_scale);
    int dst_h  = (int)((float)CANVAS_H * g_scale);
    g_offset_x = (win_w - dst_w) / 2;
    g_offset_y = (win_h - dst_h) / 2;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextureEx(g_texture,
                  (Vector2){(float)g_offset_x, (float)g_offset_y},
                  0.0f, g_scale, WHITE);
    EndDrawing();
}

/* Audio stubs — implemented in Lesson 15 */
void platform_audio_init(GameState *s, int hz)  { (void)s; (void)hz; }
void platform_audio_update(GameState *s)         { (void)s; }
void platform_audio_shutdown(void)               {}

/* -----------------------------------------------------------------------
 * MAIN LOOP
 * ----------------------------------------------------------------------- */
int main(void) {
    static GameState      state;
    static GameBackbuffer bb;

    /* Allocate the pixel buffer on the heap (640×480×4 ≈ 1.2 MB).
     * Declared static so the struct itself doesn't eat the stack, but
     * pixels must be heap-allocated so the platform can reference it
     * across function calls. */
    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;
    bb.width  = CANVAS_W;
    bb.height = CANVAS_H;
    bb.pitch  = CANVAS_W * 4;

    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);

    double prev_time = platform_get_time();

    while (!WindowShouldClose() && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f; /* clamp large dt */

        /* Clear canvas to sky blue every frame */
        int total = CANVAS_W * CANVAS_H;
        for (int i = 0; i < total; i++)
            bb.pixels[i] = COLOR_BG;

        platform_display_backbuffer(&bb);
    }

    UnloadTexture(g_texture);
    CloseWindow();
    free(bb.pixels);
    return 0;
}
```

**What's happening:**

- `malloc(CANVAS_W * CANVAS_H * sizeof(uint32_t))` — allocates once at startup.  No allocations in the loop.
- `delta_time = curr_time - prev_time` — the frame budget.  Capping at 0.1 s prevents a debugger pause from giving the simulation a huge time step.
- The pixel-clear loop writes `COLOR_BG` to every pixel each frame.  In later lessons, `game_render()` will do this.
- The R↔B swap is explained in the comments; don't try to avoid it — the channel order difference between our internal format and Raylib's GPU format is real and unavoidable.

---

## Step 4: X11 backend

X11 is more verbose but the structure is identical.

```c
/* src/main_x11.c  —  Sugar, Sugar | X11 Platform Backend (Lesson 01) */
#define _POSIX_C_SOURCE 199309L   /* expose clock_gettime, nanosleep */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>     /* clock_gettime */
#include <stdlib.h>   /* malloc, free  */
#include <string.h>   /* memset        */
#include "platform.h"

/* Platform globals — game.c never sees these. */
static Display *g_display;
static Window   g_window;
static GC       g_gc;
static XImage  *g_ximage;
static int      g_screen;
static int      g_should_quit;
static uint32_t *g_pixel_data;
static int       g_pixel_w;
static int       g_pixel_h;

void platform_init(const char *title, int width, int height) {
    g_display = XOpenDisplay(NULL);
    if (!g_display) return; /* no X server — fail silently */
    g_screen = DefaultScreen(g_display);

    g_window = XCreateSimpleWindow(
        g_display, RootWindow(g_display, g_screen),
        0, 0, (unsigned)width, (unsigned)height,
        1, BlackPixel(g_display, g_screen), WhitePixel(g_display, g_screen));

    /* Lock window size — X11 has no built-in XImage scaling. */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PMinSize | PMaxSize;
        hints->min_width  = hints->max_width  = width;
        hints->min_height = hints->max_height = height;
        XSetWMNormalHints(g_display, g_window, hints);
        XFree(hints);
    }

    /* Register for the events we care about */
    XSelectInput(g_display, g_window,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | Button1MotionMask |
                 StructureNotifyMask);

    /* Wire the window-close button (the ✕ in the title bar) */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_display, g_window, &wm_delete, 1);

    XStoreName(g_display, g_window, title);
    XMapWindow(g_display, g_window);
    XFlush(g_display);

    g_gc = XCreateGC(g_display, g_window, 0, NULL);
}

double platform_get_time(void) {
    /* CLOCK_MONOTONIC never jumps backward — correct for delta-time. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void platform_get_input(GameInput *input) {
    (void)input; /* stubbed — added in Lesson 03 */
    Atom wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);
        if (event.type == ClientMessage &&
            (Atom)event.xclient.data.l[0] == wm_delete)
            g_should_quit = 1;
    }
}

void platform_display_backbuffer(const GameBackbuffer *backbuffer) {
    if (!g_display) return;

    /* Create XImage once, pointing directly at the pixel buffer. */
    if (!g_ximage || g_pixel_w != backbuffer->width) {
        if (g_ximage) { g_ximage->data = NULL; XDestroyImage(g_ximage); }
        g_pixel_data = backbuffer->pixels;
        g_pixel_w    = backbuffer->width;
        g_pixel_h    = backbuffer->height;
        /* ZPixmap = packed pixels, same row stride as our buffer.
         * (char*)pixels — XImage stores data as char*, but it points to
         * our uint32_t array, so no conversion is needed. */
        g_ximage = XCreateImage(
            g_display, DefaultVisual(g_display, g_screen),
            (unsigned)DefaultDepth(g_display, g_screen),
            ZPixmap, 0,
            (char *)backbuffer->pixels,
            (unsigned)backbuffer->width, (unsigned)backbuffer->height,
            32, backbuffer->pitch);
    }

    /* XPutImage blits the pixel array to the window. */
    XPutImage(g_display, g_window, g_gc, g_ximage,
              0, 0, 0, 0,
              (unsigned)backbuffer->width, (unsigned)backbuffer->height);
    XFlush(g_display);
}

void platform_audio_init(GameState *s, int hz)  { (void)s; (void)hz; }
void platform_audio_update(GameState *s)         { (void)s; }
void platform_audio_shutdown(void)               {}

int main(void) {
    static GameState      state;
    static GameBackbuffer bb;

    bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
    if (!bb.pixels) return 1;
    bb.width  = CANVAS_W;
    bb.height = CANVAS_H;
    bb.pitch  = CANVAS_W * 4;

    platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H);

    double prev_time = platform_get_time();

    while (!g_should_quit && !state.should_quit) {
        double curr_time  = platform_get_time();
        float  delta_time = (float)(curr_time - prev_time);
        prev_time = curr_time;
        if (delta_time > 0.1f) delta_time = 0.1f;

        int total = CANVAS_W * CANVAS_H;
        for (int i = 0; i < total; i++)
            bb.pixels[i] = COLOR_BG;

        platform_get_input(NULL); /* processes WM_DELETE_WINDOW */
        platform_display_backbuffer(&bb);

        /* Manual 60 fps cap — X11 has no SetTargetFPS equivalent. */
        double elapsed = platform_get_time() - curr_time;
        double target  = 1.0 / 60.0;
        if (elapsed < target) {
            struct timespec sleep_ts;
            double rem = target - elapsed;
            sleep_ts.tv_sec  = (time_t)rem;
            sleep_ts.tv_nsec = (long)((rem - (double)sleep_ts.tv_sec) * 1e9);
            nanosleep(&sleep_ts, NULL);
        }
    }

    if (g_ximage) { g_ximage->data = NULL; XDestroyImage(g_ximage); }
    if (g_display) { XDestroyWindow(g_display, g_window); XCloseDisplay(g_display); }
    free(bb.pixels);
    return 0;
}
```

**X11 vs Raylib — key differences:**

| Concern         | Raylib                              | X11                                |
|-----------------|-------------------------------------|------------------------------------|
| Window creation | `InitWindow(w, h, title)`           | `XOpenDisplay` + `XCreateSimpleWindow` |
| Frame timing    | `SetTargetFPS(60)`                  | Manual `nanosleep` at end of loop  |
| Pixel upload    | `UpdateTexture` + GPU draw          | `XPutImage` direct software blit  |
| Window close    | `WindowShouldClose()`               | `WM_DELETE_WINDOW` ClientMessage  |
| Letterbox       | `DrawTextureEx` with scale          | Not done — window is fixed size   |

---

## Build and run

```bash
mkdir -p build

# Raylib
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -fsanitize=address,undefined \
      -o build/game src/main_raylib.c \
      -lraylib -lm -lpthread -ldl

./build/game

# X11
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG \
      -fsanitize=address,undefined \
      -o build/game src/main_x11.c \
      -lX11 -lm

./build/game
```

**Expected output:** A 640×480 window filled entirely with a soft sky-blue color.  Close it with Escape or the × button.

---

## Exercises

1. Change `COLOR_BG` to `GAME_RGB(30, 30, 30)` (dark grey) and rebuild.  Verify the canvas changes on both backends.
2. Add a `COLOR_ORANGE GAME_RGB(251, 140, 0)` constant to `game.h` and paint a 100×100 rectangle in the center of the canvas by writing directly to `bb.pixels` in the loop.
3. Why does X11 need `g_ximage->data = NULL` before `XDestroyImage`?  (Hint: who owns the pixel buffer?)

---

## What's next

In Lesson 02 we extract the pixel-writing code into proper drawing primitives —
`draw_pixel`, `draw_rect`, and the color macros — and pull the canvas-clear logic
into a `game_render` function.
