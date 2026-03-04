# Lesson 03: Input

## What we're building

A coloured rectangle that moves one tile per arrow-key press.  The movement
stops immediately when you release the key and never auto-repeats.  This is
exactly how the frog will hop — one tap = one hop, no holding.

We introduce the **double-buffer input pattern**: two `GameInput` structs that
trade roles each frame so "was just pressed" can be detected without any
extra state variables.

---

## What you'll learn

- `GameButtonState` — tracks both current state (`ended_down`) and transitions
  (`half_transition_count`)
- `UPDATE_BUTTON` macro — the safe way to record a key event
- `inputs[2]` double-buffer — old frame ↔ current frame swap each tick
- `prepare_input_frame` — copies held state, clears transition counts
- `GameInput` union — `buttons[4]` array AND named struct share the same memory
- Why X11 fires fake key-repeat events and how `XkbSetDetectableAutoRepeat` fixes it
- The difference between `ended_down` (is it held?) and `half_transition_count`
  (did it change this frame?)

---

## Prerequisites

- Lessons 01 + 02: window open, `draw_rect` working

---

## Concepts

### 1. `GameButtonState` — State + Transitions

JavaScript input is **event-driven**: you subscribe to `keydown` / `keyup` and
react when they fire.  In a game loop you need to **poll** each frame: is the
key held?  Was it _just_ pressed?  Did it tap-and-release within one frame?

```c
typedef struct {
    int ended_down;             /* 1 = key is currently held; 0 = released  */
    int half_transition_count;  /* how many state changes this frame         */
} GameButtonState;
```

The `half_transition_count` name comes from Casey Muratori's Handmade Hero.
A "half transition" is one state change.  A full press-and-release cycle is
two half transitions.

```
Scenario A: key held from last frame, not released this frame
  ended_down = 1
  half_transition_count = 0   ← no state change

Scenario B: key was up, pressed once this frame
  ended_down = 1
  half_transition_count = 1   ← one state change (down)

Scenario C: key pressed AND released within one frame (very fast tap)
  ended_down = 0
  half_transition_count = 2   ← two state changes (down → up)

Scenario D: key released this frame
  ended_down = 0
  half_transition_count = 1   ← one state change (up)
```

**"Just pressed" detection** (fired exactly on the first frame):

```c
// "Just pressed" = currently held AND at least one transition recorded
#define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)
```

This is equivalent to a JS `keydown` event where `event.repeat === false`:

```js
// JS:
document.addEventListener('keydown', e => {
  if (!e.repeat) { hop(); }   // fires only on first press, not on auto-repeat
});
```

### 2. `UPDATE_BUTTON` Macro

```c
/* Call on KeyPress:   UPDATE_BUTTON(btn, 1)
   Call on KeyRelease: UPDATE_BUTTON(btn, 0)
   Only increments count when state actually changes.                    */
#define UPDATE_BUTTON(button, is_down)                     \
    do {                                                    \
        if ((button).ended_down != (is_down)) {             \
            (button).half_transition_count++;               \
            (button).ended_down = (is_down);                \
        }                                                   \
    } while (0)
```

The `if (ended_down != is_down)` guard prevents double-counting.  Without it,
repeated `KeyPress` events from the OS (before we disable auto-repeat) would
increment the count even when the state hasn't changed.

```js
// JS analogy:
// function updateButton(btn, isDown) {
//   if (btn.endedDown !== isDown) {
//     btn.halfTransitionCount++;
//     btn.endedDown = isDown;
//   }
// }
```

### 3. `GameInput` — Union of Array and Named Struct

We need two views of the same four buttons:
- **Array view** (`buttons[4]`): for `prepare_input_frame` loop iteration
- **Named view** (`move_up`, `move_down`, etc.): for readable game logic

A C `union` overlays both at the same memory address:

```c
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];  /* iterate for reset     */
        struct {
            GameButtonState move_up;            /* readable in game code */
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;
        };
    };
    int quit;     /* set to 1 to exit the game loop                      */
    int restart;  /* set to 1 to restart after game over                 */
} GameInput;
```

`buttons[0]` and `move_up` occupy the same bytes.  Writing through one name
reads correctly through the other:

```c
input.buttons[0].ended_down = 1;
// input.move_up.ended_down is now also 1 — same memory

// JS analogy:
// const input = {
//   buttons: [moveUp, moveDown, moveLeft, moveRight],
//   get moveUp()    { return this.buttons[0]; },
//   get moveDown()  { return this.buttons[1]; },
//   // ...
// };
```

### 4. `inputs[2]` — The Double-Buffer Pattern

We keep **two** `GameInput` structs: `inputs[0]` (previous frame) and
`inputs[1]` (current frame).  At the start of each frame:

1. **Swap** — `inputs[0]` ↔ `inputs[1]`  (`inputs[0]` now = last frame's data)
2. **Prepare** — copy `ended_down` from `inputs[0]` into `inputs[1]`;
   clear `half_transition_count` to 0
3. **Read hardware** — poll / receive events; call `UPDATE_BUTTON` into `inputs[1]`
4. **Update game** — `game_update(&state, &inputs[1], dt)`

After step 2, a key that was held last frame has `ended_down=1` and
`half_transition_count=0`.  A new press increments `half_transition_count` to 1.
The game can now distinguish "first frame of a press" from "key held since last frame".

```js
// JS analogy: manual "last frame" tracking
// let prevInput = { isDown: false };
// let currInput = { isDown: false };
// function tick() {
//   const tmp = prevInput; prevInput = currInput; currInput = tmp;
//   currInput.isDown = prevInput.isDown;  // carry held state forward
//   currInput.justPressed = false;
//   // ... then read hardware events into currInput
// }
```

### 5. `prepare_input_frame`

```c
/* Copies ended_down from old frame; resets half_transition_count.
   Called BEFORE reading new hardware events.                            */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
    int btn;
    for (btn = 0; btn < BUTTON_COUNT; btn++) {
        current_input->buttons[btn].ended_down =
            old_input->buttons[btn].ended_down;
        current_input->buttons[btn].half_transition_count = 0;
    }
    current_input->quit    = 0;
    current_input->restart = 0;
}
```

This is the function that makes "key held across frames" work without any new
events arriving.  If no X11 `KeyPress` event fires for a held key (because the
OS may not generate repeat events), the key is still recorded as held because
we copied `ended_down` forward.

### 6. X11 Auto-Repeat Suppression

On X11, when you hold a key the OS generates synthetic `KeyRelease` + `KeyPress`
pairs at the keyboard repeat rate (typically 30–50 Hz).  Without suppression,
those fake pairs flood `half_transition_count` and cause the frog to teleport
instead of hop.

```c
/* In platform_init, after XMapWindow: */
XkbSetDetectableAutoRepeat(g_display, True, NULL);
```

With detectable auto-repeat **enabled**, X11 stops sending the fake release/press
pairs.  Key-repeat events arrive as a steady stream of `KeyPress` events without
intervening `KeyRelease` — but our `UPDATE_BUTTON` guard ignores them because
`ended_down` is already `1` (no state change).

> **Course Note:** The reference Frogger source already includes
> `XkbSetDetectableAutoRepeat`.  It is highlighted here because it is subtle —
> the game looks fine in short testing sessions but breaks badly on held keys
> without it.

---

## Step 1 — Updated `src/platform.h` (Lesson 03 stage)

Add `BUTTON_COUNT`, `UPDATE_BUTTON`, and `prepare_input_frame`:

```c
/* platform.h — Platform API Contract (Lesson 03) */
#ifndef FROGGER_PLATFORM_H
#define FROGGER_PLATFORM_H

#include "utils/backbuffer.h"

/* ── Input System ────────────────────────────────────────────────────────── */

typedef struct {
    int ended_down;
    int half_transition_count;
} GameButtonState;

#define BUTTON_COUNT 4

/* Call on KeyPress (is_down=1) or KeyRelease (is_down=0).
   Only increments count when the state actually changes.                 */
#define UPDATE_BUTTON(button, is_down)                     \
    do {                                                    \
        if ((button).ended_down != (is_down)) {             \
            (button).half_transition_count++;               \
            (button).ended_down = (is_down);                \
        }                                                   \
    } while (0)

typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];
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

/* Reset half_transition_count; copy ended_down from old → current.
   Call BEFORE reading hardware events each frame.                       */
void prepare_input_frame(GameInput *old_input, GameInput *current_input);

/* ── PlatformGameProps ────────────────────────────────────────────────── */
typedef struct {
    const char *title;
    int         width;
    int         height;
    Backbuffer  backbuffer;
    int         is_running;
} PlatformGameProps;

/* ── Four-function Platform Contract ─────────────────────────────────── */
int    platform_init(PlatformGameProps *props);
double platform_get_time(void);
void   platform_get_input(GameInput *input, PlatformGameProps *props);
void   platform_shutdown(void);

static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    GameInput tmp = *a; *a = *b; *b = tmp;
}

#endif /* FROGGER_PLATFORM_H */
```

---

## Step 2 — `prepare_input_frame` implementation

Add a minimal `src/input.c` (or place in `game.c` in Lesson 04):

```c
/* input.c — prepare_input_frame implementation */
#include "platform.h"

void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
    int btn;
    for (btn = 0; btn < BUTTON_COUNT; btn++) {
        current_input->buttons[btn].ended_down =
            old_input->buttons[btn].ended_down;
        current_input->buttons[btn].half_transition_count = 0;
    }
    current_input->quit    = 0;
    current_input->restart = 0;
}
```

---

## Step 3 — Raylib `main_raylib.c` (Lesson 03 stage)

```c
/* main_raylib.c — Raylib Backend (Lesson 03: input + moving rect) */
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

/* Raylib uses IsKeyDown for held detection.  We compare against
   last frame's ended_down to detect transitions.                        */
void platform_get_input(GameInput *input, PlatformGameProps *props) {
    #define MAP_KEY(BTN, K1, K2) \
        do { int held = IsKeyDown(K1) || IsKeyDown(K2); \
             UPDATE_BUTTON(BTN, held); } while(0)

    MAP_KEY(input->move_up,    KEY_UP,    KEY_W);
    MAP_KEY(input->move_down,  KEY_DOWN,  KEY_S);
    MAP_KEY(input->move_left,  KEY_LEFT,  KEY_A);
    MAP_KEY(input->move_right, KEY_RIGHT, KEY_D);
    #undef MAP_KEY

    if (IsKeyPressed(KEY_R))       input->restart = 1;
    if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q))
        { input->quit = 1; props->is_running = 0; }
}

void platform_shutdown(void) { UnloadTexture(g_texture); CloseWindow(); }

int main(void) {
    PlatformGameProps props;
    memset(&props, 0, sizeof(props));
    props.title = "Frogger"; props.width = SCREEN_W; props.height = SCREEN_H;
    if (!platform_init(&props)) return 1;

    /* Moving rectangle position (in tiles) */
    int rect_tile_x = 8;
    int rect_tile_y = 9;

    GameInput inputs[2];
    memset(inputs, 0, sizeof(inputs));

    while (!WindowShouldClose() && props.is_running) {
        /* ── Input double-buffer swap + prepare ──────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        GameInput *in = &inputs[1];

        /* ── "Just pressed" detection: one hop per key press ─────────── */
        /* JUST_PRESSED fires only on the first frame of a press.
           ended_down=1 means key is currently held.
           half_transition_count>0 means it changed state this frame.    */
        #define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)

        if (JUST_PRESSED(in->move_up)    && rect_tile_y > 0)
            rect_tile_y--;
        if (JUST_PRESSED(in->move_down)  && rect_tile_y < (SCREEN_CELLS_H / 8) - 1)
            rect_tile_y++;
        if (JUST_PRESSED(in->move_left)  && rect_tile_x > 0)
            rect_tile_x--;
        if (JUST_PRESSED(in->move_right) && rect_tile_x < (SCREEN_CELLS_W / 8) - 1)
            rect_tile_x++;

        /* ── Render ──────────────────────────────────────────────────── */
        Backbuffer *bb = &props.backbuffer;
        draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_DARK_GREY);

        /* Draw grid lines to visualise tile boundaries */
        int i;
        for (i = 0; i < SCREEN_CELLS_W; i += 8)
            draw_rect(bb, i * CELL_PX, 0, 1, bb->height, COLOR_GREY);
        for (i = 0; i < SCREEN_CELLS_H; i += 8)
            draw_rect(bb, 0, i * CELL_PX, bb->width, 1, COLOR_GREY);

        /* Draw the moving rectangle at the current tile position        */
        draw_rect(bb,
                  rect_tile_x * 8 * CELL_PX,   /* x = tile * tile_width_px */
                  rect_tile_y * 8 * CELL_PX,   /* y = tile * tile_height_px */
                  8 * CELL_PX,                  /* width = one tile = 64 px  */
                  8 * CELL_PX,                  /* height = one tile = 64 px */
                  COLOR_GREEN);

        /* Upload + display */
        UpdateTexture(g_texture, g_pixel_buf);
        {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float sc = ((float)sw/SCREEN_W < (float)sh/SCREEN_H)
                       ? (float)sw/SCREEN_W : (float)sh/SCREEN_H;
            int vw = (int)(SCREEN_W*sc), vh = (int)(SCREEN_H*sc);
            int vx = (sw-vw)/2, vy = (sh-vh)/2;
            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(g_texture,
                (Rectangle){0,0,SCREEN_W,SCREEN_H},
                (Rectangle){(float)vx,(float)vy,(float)vw,(float)vh},
                (Vector2){0,0}, 0, WHITE);
            EndDrawing();
        }
    }

    platform_shutdown();
    return 0;
}
```

---

## Step 4 — X11 `main_x11.c` (Lesson 03 stage)

Key additions: `XkbSetDetectableAutoRepeat` in `platform_init`, full key
mapping in `platform_get_input`, and the same moving-rect logic in `main`.

```c
/* main_x11.c — X11 Backend (Lesson 03: input + moving rect) */
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
static int         g_win_w = SCREEN_W, g_win_h = SCREEN_H;
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

    /* ── Suppress fake key-repeat events ─────────────────────────────
       Without this, holding a key generates synthetic KeyRelease+KeyPress
       pairs at the OS repeat rate, making half_transition_count spike.  */
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
                int is_press = (ev.type == KeyPress);
                /* XkbKeycodeToKeysym: correct key name even with layout remapping */
                KeySym sym = XkbKeycodeToKeysym(g_display,
                                                ev.xkey.keycode, 0, 0);
                GameButtonState *btn = NULL;
                switch (sym) {
                    case XK_Up:    case XK_w: case XK_W: btn = &input->move_up;    break;
                    case XK_Down:  case XK_s: case XK_S: btn = &input->move_down;  break;
                    case XK_Left:  case XK_a: case XK_A: btn = &input->move_left;  break;
                    case XK_Right: case XK_d: case XK_D: btn = &input->move_right; break;
                    case XK_r: case XK_R:
                        if (is_press) input->restart = 1;
                        break;
                    case XK_Escape: case XK_q: case XK_Q:
                        input->quit = 1; props->is_running = 0;
                        break;
                    default: break;
                }
                if (btn) UPDATE_BUTTON(*btn, is_press);
                break;
            }
        }
    }
}

void platform_shutdown(void) {
    if (g_texture) { glDeleteTextures(1, &g_texture); g_texture = 0; }
    if (g_gl_ctx)  { glXMakeCurrent(g_display, None, NULL);
                     glXDestroyContext(g_display, g_gl_ctx); g_gl_ctx = NULL; }
    if (g_window)  { XDestroyWindow(g_display, g_window);  g_window  = 0; }
    if (g_display) { XCloseDisplay(g_display); g_display = NULL; }
}

static void display_backbuffer(void) {
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, g_pixel_buf);
    float sx = (float)g_win_w/SCREEN_W, sy = (float)g_win_h/SCREEN_H;
    float sc = (sx < sy) ? sx : sy;
    int vw=(int)(SCREEN_W*sc), vh=(int)(SCREEN_H*sc);
    int vx=(g_win_w-vw)/2, vy=(g_win_h-vh)/2;
    glViewport(0,0,g_win_w,g_win_h);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glViewport(vx,vy,vw,vh);
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

    int rect_tile_x = 8, rect_tile_y = 9;
    GameInput inputs[2]; memset(inputs, 0, sizeof(inputs));
    double t_last = platform_get_time();

    while (props.is_running) {
        double t_now = platform_get_time();
        double dt = t_now - t_last; t_last = t_now;
        if (dt > 0.066) dt = 0.066;
        (void)dt;

        /* ── Input double-buffer ─────────────────────────────────────── */
        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        GameInput *in = &inputs[1];

        #define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)
        if (JUST_PRESSED(in->move_up)    && rect_tile_y > 0) rect_tile_y--;
        if (JUST_PRESSED(in->move_down)  && rect_tile_y < (SCREEN_CELLS_H/8)-1) rect_tile_y++;
        if (JUST_PRESSED(in->move_left)  && rect_tile_x > 0) rect_tile_x--;
        if (JUST_PRESSED(in->move_right) && rect_tile_x < (SCREEN_CELLS_W/8)-1) rect_tile_x++;
        #undef JUST_PRESSED

        Backbuffer *bb = &props.backbuffer;
        draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_DARK_GREY);

        int i;
        for (i = 0; i < SCREEN_CELLS_W; i += 8)
            draw_rect(bb, i*CELL_PX, 0, 1, bb->height, COLOR_GREY);
        for (i = 0; i < SCREEN_CELLS_H; i += 8)
            draw_rect(bb, 0, i*CELL_PX, bb->width, 1, COLOR_GREY);

        draw_rect(bb, rect_tile_x*8*CELL_PX, rect_tile_y*8*CELL_PX,
                  8*CELL_PX, 8*CELL_PX, COLOR_GREEN);

        display_backbuffer();
    }

    platform_shutdown();
    return 0;
}
```

---

## Build and Run

```bash
chmod +x build-dev.sh

# Raylib
./build-dev.sh -r

# X11
./build-dev.sh --backend=x11 -r
```

---

## Expected Result

A dark-grey grid appears.  A green 64×64 pixel square starts at column 8,
row 9 (centre of the bottom strip).

- Press **↑ W** — square moves one tile up.  Tap rapidly; each press moves
  exactly once.
- Press **↓ S ← A → D** — square moves in all four directions.
- Hold a key — the square does NOT keep moving (no auto-repeat).
- The square stops at the grid boundary and does not wrap.

Both X11 and Raylib behave identically.

---

## Key Design Principle: One Hop per Press

This design is intentional for Frogger — the frog hops **discretely**, one
tile per keypress.  Compare to Tetris DAS/ARR (delayed auto-shift / auto-repeat
rate) where holding left/right eventually triggers repeated movement.  Frogger
needs no DAS because the player controls each hop explicitly.

```c
// JUST_PRESSED: fires on the FIRST frame a key goes down
// Equivalent to: event.type === 'keydown' && !event.repeat
#define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)
```

The `half_transition_count > 0` part is the key: after the first frame, even
with the key held down, `half_transition_count` is 0 (no new transitions), so
`JUST_PRESSED` evaluates false until the key is released and pressed again.

---

## Exercises

1. Add a diagonal movement: if both UP and RIGHT are just-pressed in the same
   frame, move diagonally.  (Hint: check both `JUST_PRESSED` conditions without
   `else`.)
2. Print `half_transition_count` to `stderr` when it exceeds 1 — try tapping a
   key very quickly to see a count of 2.
3. Temporarily remove `XkbSetDetectableAutoRepeat` and hold a key for 2 seconds
   — observe the rectangle teleporting instead of staying still.
4. Change `JUST_PRESSED` to `btn.ended_down` (remove the transition check) and
   verify the rectangle now slides continuously when a key is held.

---

## What's next

**Lesson 04** introduces `GameState`, `GAME_PHASE`, and the
`game_init` / `game_update` / `game_render` split.  The moving rectangle
becomes a frog positioned at tile `(8, 9)`, and we show the current phase as
debug text on screen.
