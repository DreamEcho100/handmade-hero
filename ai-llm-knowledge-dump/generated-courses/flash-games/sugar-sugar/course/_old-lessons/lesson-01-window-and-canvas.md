# Lesson 01 — Window and Canvas

## What you build

A 640×480 window that opens on screen and fills itself with a solid sky-blue background color each frame — the blank canvas on which the entire game will later be drawn.

## Concepts introduced

- The `GameBackbuffer` struct: pixel buffer, width, height, pitch
- Heap allocation for the pixel buffer with `malloc`
- `static` storage class — BSS segment vs stack
- The `platform_init` / `platform_display_backbuffer` platform contract
- The four-step game loop: `prepare_input_frame` → `get_input` → `update` → `render` → `display`
- The `build-dev.sh` unified build script: `--backend=x11` vs `--backend=raylib`
- Raylib letterbox scaling
- `COLOR_BG` and the `GAME_RGB` macro

---

## JS → C analogy

| JavaScript / Canvas 2D                              | C equivalent                                      |
|-----------------------------------------------------|---------------------------------------------------|
| `const canvas = document.createElement('canvas')`  | `platform_init("Sugar, Sugar", CANVAS_W, CANVAS_H)` |
| `canvas.width = 640; canvas.height = 480`           | `#define CANVAS_W 640` / `#define CANVAS_H 480`  |
| `new Uint32Array(640 * 480)`                        | `malloc(CANVAS_W * CANVAS_H * sizeof(uint32_t))`  |
| `imageData.data[y * width + x]` (4 bytes per pixel) | `bb.pixels[y * width + x]` (1 `uint32_t` per pixel) |
| `ctx.putImageData(imageData, 0, 0)`                 | `platform_display_backbuffer(&bb)`                |
| Module-level `let state = {}`                       | `static GameState state;` (BSS segment)           |
| `requestAnimationFrame` loop                        | `while (!quit)` loop with `platform_get_time()`   |
| `elapsed / 1000` (ms → s)                           | `(float)(curr_time - prev_time)` (already seconds)|

---

## Step-by-step

### Step 1: Understand the file layout

Before writing a single line, orient yourself in the project:

```
src/
  game.h          ← shared types and public game API (GameBackbuffer, GameState, …)
  platform.h      ← platform contract: init / get_time / get_input / display_backbuffer
  game.c          ← all game logic (physics, rendering) — zero OS calls
  main_x11.c      ← X11 platform backend (implements platform.h)
  main_raylib.c   ← Raylib platform backend (implements platform.h)
  levels.c        ← level definitions
build-dev.sh      ← unified build script
```

The key insight is the **separation of concerns**:

- `game.c` never calls X11 or Raylib. It only calls functions declared in `platform.h`.
- `main_x11.c` and `main_raylib.c` each implement those functions independently.
- To add a new platform (SDL3, WebAssembly, Win32) you write a new `main_*.c` — `game.c` is untouched.

### Step 2: Read the `GameBackbuffer` struct

Open `src/game.h`. The struct is defined as:

```c
typedef struct {
  uint32_t *pixels; /* flat ARGB array: pixels[y * width + x] = color  */
  int width;        /* canvas width  in pixels (640)                    */
  int height;       /* canvas height in pixels (480)                    */
  int pitch;        /* bytes per row = width * 4                        */
} GameBackbuffer;
```

**Why `pitch`?**

`pitch` is the number of **bytes** per row, not pixels. For a 640-pixel-wide canvas with 4 bytes per pixel:

```
pitch = 640 * 4 = 2560 bytes
```

This matters because:
1. `XCreateImage` in X11 needs `bytes_per_line` (= pitch), not pixel count.
2. Some hardware/APIs require rows to be aligned to 64 or 128 bytes — `pitch >= width*4` in those cases.
3. Here `pitch == width * 4` exactly, but the field exists so the rendering code works correctly on both backends.

**The common mistake:** writing `pitch = width` instead of `pitch = width * 4`. The X11 backend will display a garbled image where every row is offset by 3× the expected amount.

### Step 3: See how `game_init` sets the backbuffer dimensions

In `src/game.c`, `game_init` sets:

```c
void game_init(GameState *state, GameBackbuffer *backbuffer) {
  memset(state, 0, sizeof(*state));

  backbuffer->width  = CANVAS_W;       /* 640 */
  backbuffer->height = CANVAS_H;       /* 480 */
  backbuffer->pitch  = CANVAS_W * 4;   /* 2560 bytes per row */

  state->unlocked_count = 1;
  state->gravity_sign   = 1;
  state->title_hover    = -1;

  change_phase(state, PHASE_TITLE);
}
```

Notice that `game_init` does **not** allocate `bb.pixels`. The platform main allocates it before calling `game_init` — because only the platform knows where memory should come from (heap, arena, OS-mapped, etc.).

### Step 4: Understand `static` storage — BSS vs stack

In both `main_x11.c` and `main_raylib.c` the main function starts with:

```c
int main(void) {
    static GameState      state;
    static GameBackbuffer bb;
    ...
```

**Why `static`?**

`GameState` contains a `GrainPool` with `float x[4096]`, `float y[4096]`, etc., and a `LineBitmap` with `uint8_t pixels[640*480]`. Together that is roughly **460 KB**. The default stack on Linux is only 8 MB, but calling into deeply nested functions can easily overflow it.

`static` local variables are placed in the **BSS segment** (zero-initialised at program startup), alongside global variables. They are not pushed/popped on the call stack — they exist for the entire lifetime of the process.

| Storage class  | Where in memory | Lifetime        | Zero-initialised |
|----------------|-----------------|-----------------|------------------|
| `int x;` (local)       | Stack          | Function call   | No               |
| `static int x;` (local) | BSS/Data      | Program lifetime | Yes             |
| `int x;` (global)      | BSS/Data       | Program lifetime | Yes              |

**JS analogy:** In JS, declaring `const state = {}` at module level keeps it alive forever. A `static` local in C is the same idea — module-level lifetime, function-level scope.

### Step 5: Allocate the pixel buffer on the heap

Right after the `static` declarations:

```c
/* Allocate the pixel buffer on the heap.
 * 640 × 480 × 4 bytes = 1,228,800 bytes ≈ 1.2 MB. */
bb.pixels = (uint32_t *)malloc((size_t)(CANVAS_W * CANVAS_H) * sizeof(uint32_t));
if (!bb.pixels) return 1; /* out of memory */
```

Breaking this down:
- `CANVAS_W * CANVAS_H` = 307,200 pixels
- `sizeof(uint32_t)` = 4 bytes
- Total: 1,228,800 bytes ≈ 1.2 MB

The cast `(uint32_t *)` is required in C when the result of `malloc` (which returns `void *`) is assigned to a typed pointer. (In C++ it would not compile without the cast.)

**Why heap and not the BSS `static` pixel array directly in `GameBackbuffer`?**

`GameBackbuffer` is a small struct — 16 bytes. Its `pixels` field is a *pointer*. The 1.2 MB buffer lives separately on the heap. This keeps the struct small and lets the platform control where pixels live.

### Step 6: The platform contract — four functions

Open `src/platform.h`. Every platform backend must implement exactly these:

```c
void   platform_init(const char *title, int width, int height);
double platform_get_time(void);
void   platform_get_input(GameInput *input);
void   platform_display_backbuffer(const GameBackbuffer *backbuffer);
```

The game loop in `main_x11.c` calls them in order every frame:

```c
while (!g_should_quit && !state.should_quit) {
    double curr_time  = platform_get_time();
    float  delta_time = (float)(curr_time - prev_time);
    prev_time = curr_time;

    if (delta_time > 0.1f) delta_time = 0.1f;  /* debugger-pause guard */

    /* Step 1: clear per-frame input counters */
    prepare_input_frame(&input);

    /* Step 2: read OS events → fill input struct */
    platform_get_input(&input);

    /* Step 3: advance the simulation */
    game_update(&state, &input, delta_time);

    /* Step 4: draw the current frame into the pixel buffer */
    game_render(&state, &bb);

    /* Step 5: send the pixel buffer to the X11 window */
    platform_display_backbuffer(&bb);
}
```

### Step 7: How `platform_display_backbuffer` works in X11

In `src/main_x11.c`, the first call to `platform_display_backbuffer` creates an `XImage` that wraps the pixel buffer — **no copy**, just a pointer:

```c
g_ximage = XCreateImage(
    g_display,
    DefaultVisual(g_display, g_screen),
    (unsigned)DefaultDepth(g_display, g_screen),
    ZPixmap,
    0,                          /* offset */
    (char *)backbuffer->pixels, /* pointer into our pixel buffer */
    (unsigned)backbuffer->width,
    (unsigned)backbuffer->height,
    32,                         /* bitmap_pad: 32-bit aligned */
    backbuffer->pitch           /* bytes per row = width * 4 */
);
if (g_ximage) g_ximage->byte_order = LSBFirst;
```

Every subsequent call blits the `XImage` to the window:

```c
XPutImage(g_display, g_window, g_gc, g_ximage,
          0, 0,   /* src x, y */
          0, 0,   /* dst x, y */
          (unsigned)backbuffer->width,
          (unsigned)backbuffer->height);
XFlush(g_display);
```

**JS analogy:** `XPutImage` is like `ctx.putImageData(imageData, 0, 0)`. Both copy a flat pixel array to the display surface.

The X11 window is locked to exactly `CANVAS_W × CANVAS_H` using `XSizeHints` — there is no scaling. This is why you see the fixed 640×480 canvas at all times.

### Step 8: How `platform_display_backbuffer` works in Raylib (letterbox)

Raylib supports window resizing. In `src/main_raylib.c`, the backbuffer is uploaded to a GPU texture, then drawn with letterbox scaling:

```c
/* Letterbox: scale the 640×480 canvas to fit the current window while
 * preserving aspect ratio, centering it with black borders. */
int   win_w = GetScreenWidth();
int   win_h = GetScreenHeight();
float scale  = (float)win_w / (float)CANVAS_W;
float scaleH = (float)win_h / (float)CANVAS_H;
if (scaleH < scale) scale = scaleH;   /* pick the smaller scale factor */
int dst_w = (int)((float)CANVAS_W * scale);
int dst_h = (int)((float)CANVAS_H * scale);
int dst_x = (win_w - dst_w) / 2;
int dst_y = (win_h - dst_h) / 2;

BeginDrawing();
ClearBackground(BLACK);       /* fills the border areas with black */
DrawTextureEx(g_texture,
              (Vector2){(float)dst_x, (float)dst_y},
              0.0f,            /* rotation */
              scale,
              WHITE);
EndDrawing();
```

The logic finds the largest `scale` that fits both axes without cropping, then centers the result. The black `ClearBackground` fills the unused border bars.

### Step 9: Filling the canvas with `COLOR_BG`

In `src/game.c`, every render function starts by clearing the backbuffer. For `PHASE_PLAYING` (and as your first exercise, the blank canvas):

```c
/* Clear to background color */
for (int i = 0; i < bb->width * bb->height; i++)
    bb->pixels[i] = COLOR_BG;
```

`COLOR_BG` is defined in `game.h` as:

```c
#define COLOR_BG  GAME_RGB(135, 195, 225)  /* soft sky-blue */
```

Expanding `GAME_RGB`:

```c
#define GAME_RGB(r, g, b)  GAME_RGBA(r, g, b, 0xFF)
#define GAME_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
```

So `GAME_RGB(135, 195, 225)` evaluates to `0xFF87C3E1`. In memory (little-endian):

```
bytes: [0xE1, 0xC3, 0x87, 0xFF]
         B     G     R     A
```

X11 reads these bytes as BGR (alpha ignored) — correct! Blue=0xE1, Green=0xC3, Red=0x87 → sky blue.

### Step 10: Build and run

The `build-dev.sh` script always produces a **debug build** at `./build/game`:

```bash
# Raylib backend (default) — debug, no run
./build-dev.sh

# X11 backend — debug, no run
./build-dev.sh --backend=x11

# Build Raylib and immediately run (-r = run after build)
./build-dev.sh -r

# Build X11 and immediately run
./build-dev.sh --backend=x11 -r
```

Output is **always** `./build/game`:

| Command                           | Output         |
|-----------------------------------|----------------|
| `./build-dev.sh`                  | `build/game`   |
| `./build-dev.sh --backend=x11`    | `build/game`   |
| `./build-dev.sh -r`               | runs `build/game` |

Run manually after building:

```bash
./build/game
```

You should see a 640×480 window filled with solid sky-blue (`#87C3E1`).

> **Why always debug?** `build-dev.sh` is your daily development tool.
> Debug flags (`-DDEBUG -fsanitize=address,undefined`) catch bugs immediately.
> For optimised release builds, use a separate `build-release.sh`.

---

## Common mistakes to prevent

1. **`pitch = width` instead of `pitch = width * 4`.**
   `pitch` is **bytes per row**. Each pixel is 4 bytes (`uint32_t`). Writing `pitch = width` makes `XCreateImage` think rows are 4× shorter than they are — the image displays as a garbled slanted stripe.

2. **Not checking `malloc` return value.**
   On OOM, `malloc` returns `NULL`. Writing to `bb.pixels` when it is `NULL` is undefined behaviour — typically a segfault. Always guard with `if (!bb.pixels) return 1;`.

3. **Declaring `GameState state;` on the stack (not `static`).**
   `GameState` is ~460 KB. The stack default is 8 MB, but nested function calls eat into it quickly. Always declare it `static` or allocate on the heap.

4. **Calling `game_update`/`game_render` before `platform_init`.**
   `platform_init` opens the display and sets up the graphics context. If `platform_display_backbuffer` is called first, `g_display` is `NULL` and the X11 backend segfaults.

5. **Forgetting `XFlush` in the X11 backend.**
   Without `XFlush`, X11 may buffer the `XPutImage` call and not actually update the window until the buffer fills. The display appears frozen.

6. **Raylib: forgetting `BeginDrawing()` / `EndDrawing()` around the draw call.**
   `DrawTextureEx` must be bracketed by these calls. Without them, Raylib's render state machine is in an invalid state and the texture is never drawn.

---

## What to run

```bash
# Build Raylib backend (default)
chmod +x build-dev.sh
./build-dev.sh

# Build and run in one step
./build-dev.sh -r

# Build X11 and run
./build-dev.sh --backend=x11 -r

# Run manually after any build
./build/game
```

Expected result: a 640×480 window with a solid sky-blue fill. Closing the window exits cleanly.

AddressSanitizer and UndefinedBehaviorSanitizer are **always active** in dev builds.
If there are any out-of-bounds writes, ASan will print a stack trace and exit.

---

## Summary

This lesson establishes the three foundational pieces every frame needs: a `GameBackbuffer` struct that describes a flat `uint32_t` pixel array (`pixels[y * width + x]`), a platform that can display it (`platform_display_backbuffer` via `XPutImage` or `DrawTextureEx`), and a game loop that ticks `game_update → game_render → display` at ~60 fps. The `pitch = width * 4` rule is the single most important detail to internalize — it converts pixel-count rows into byte-count rows, which every display API requires. With both X11 and Raylib backends wired up via the unified `build-dev.sh`, you can swap platforms without touching any game logic.
