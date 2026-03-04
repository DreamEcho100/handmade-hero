# Lesson 02: Pixel Rendering — Backbuffer, Colors, and Drawing Primitives

## What we're building

Move the raw pixel-write loop from `main()` into proper drawing functions, and
give every color a name.  By the end you will have `draw_pixel`, `draw_rect`,
`draw_rect_outline`, and the full color-constant table — the building blocks
every later lesson uses.

## What you'll learn

- The `0xAARRGGBB` pixel format and why we chose it
- How to write a bounds-checked `draw_pixel` with a single unsigned cast trick
- How `draw_rect` clips to the canvas edge (so you can pass negative coordinates without crashing)
- Why Raylib needs a R↔B swap but X11 does not
- How to separate rendering from game logic using `game_render()`

## Prerequisites

- Lesson 01 complete and building on both backends

---

## Step 1: Expand `game.h` with colors and drawing stubs

```c
/* src/game.h  (Lesson 02 additions — show only the new/changed parts) */

/* -----------------------------------------------------------------------
 * COLOR SYSTEM  —  pixel format 0xAARRGGBB
 *
 * In memory on a little-endian CPU the bytes are stored as [B, G, R, A].
 * X11's XImage reads them in that order (BGR), ignoring alpha — exactly right.
 * Raylib's GPU texture uses RGBA order, so we swap R↔B before calling
 * UpdateTexture and swap back afterward (see platform_display_backbuffer).
 *
 * JS analogy: like CSS hex color but with the alpha byte prepended:
 *   CSS  #RRGGBB
 *   ARGB 0xAARRGGBB
 * ----------------------------------------------------------------------- */
#define GAME_RGBA(r, g, b, a) \
  (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | \
   ((uint32_t)(g) <<  8) |  (uint32_t)(b))
#define GAME_RGB(r, g, b)  GAME_RGBA(r, g, b, 0xFF)

/* Named color constants — one definition here, usable everywhere. */
#define COLOR_BG           GAME_RGB(135, 195, 225)  /* sky blue canvas      */
#define COLOR_LINE         GAME_RGB( 50,  50,  50)  /* player-drawn lines   */
#define COLOR_OBSTACLE     GAME_RGB(130, 110,  80)  /* level obstacles      */
#define COLOR_CUP_BORDER   GAME_RGB( 60,  60, 100)  /* cup outline          */
#define COLOR_CUP_FILL     GAME_RGB(180, 210, 255)  /* cup fill bar         */
#define COLOR_CUP_FILL_FULL GAME_RGB(80, 200, 100)  /* cup fully filled     */
#define COLOR_CUP_EMPTY    GAME_RGB(220, 220, 240)  /* cup empty area       */
#define COLOR_UI_TEXT      GAME_RGB( 50,  50,  50)  /* labels and numbers   */
#define COLOR_BTN_NORMAL   GAME_RGB(210, 200, 190)  /* button background    */
#define COLOR_BTN_HOVER    GAME_RGB(180, 170, 160)  /* button hover state   */
#define COLOR_BTN_BORDER   GAME_RGB(120, 110, 100)  /* button outline       */
#define COLOR_WHITE        GAME_RGB(255, 255, 255)
#define COLOR_BLACK        GAME_RGB(  0,   0,   0)
#define COLOR_RED          GAME_RGB(229,  57,  53)  /* grain / filter red   */
#define COLOR_GREEN        GAME_RGB( 67, 160,  71)  /* grain / filter green */
#define COLOR_ORANGE       GAME_RGB(251, 140,   0)  /* grain / filter orange*/
#define COLOR_CREAM        GAME_RGB(232, 213, 183)  /* default grain color  */
#define COLOR_PORTAL_A     GAME_RGB(100, 180, 255)  /* teleporter entry     */
#define COLOR_PORTAL_B     GAME_RGB(255, 140,   0)  /* teleporter exit      */
#define COLOR_COMPLETE     GAME_RGB( 76, 175,  80)  /* level-complete panel */
```

**What's happening:**

- `GAME_RGBA` builds a 32-bit value by shifting each channel into its byte position.  `GAME_RGB` just fixes alpha to 0xFF.
- All constants live in the header so any `.c` file that `#include`s `game.h` gets them automatically — no duplication.

---

## Step 2: Drawing primitives in `game.c`

These are `static` functions (internal to `game.c`) until you decide they need to be shared across files.

```c
/* src/game.c  —  drawing primitives (add near the top, after includes) */

/* draw_pixel: write a single pixel.
 *
 * The unsigned cast trick:
 *   (unsigned)x < (unsigned)width
 * When x is negative, casting to unsigned wraps it to a huge positive number
 * (e.g. -1 → 4294967295), which is always >= width.  So one comparison handles
 * both "x < 0" and "x >= width" without a branch.
 *
 * JS analogy: like ctx.fillRect(x, y, 1, 1) but O(1) and zero-overhead. */
static void draw_pixel(GameBackbuffer *bb, int x, int y, uint32_t color) {
  if ((unsigned)x < (unsigned)bb->width &&
      (unsigned)y < (unsigned)bb->height)
    bb->pixels[y * bb->width + x] = color;
}

/* draw_rect: fill an axis-aligned rectangle.
 *
 * Clips against the canvas edge so callers can pass coordinates that
 * partially exceed the canvas without crashing.  The inner loop writes
 * a whole row at once — the compiler will vectorise this with SIMD. */
static void draw_rect(GameBackbuffer *bb, int x, int y, int w, int h,
                      uint32_t color) {
  /* Clamp to canvas bounds — don't let the fill overflow the pixel array. */
  int x0 = (x < 0)             ? 0          : x;
  int y0 = (y < 0)             ? 0          : y;
  int x1 = (x + w > bb->width) ? bb->width  : x + w;
  int y1 = (y + h > bb->height)? bb->height : y + h;
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++)
      row[px] = color;
  }
}

/* draw_rect_outline: four 1-pixel-thick edges. */
static void draw_rect_outline(GameBackbuffer *bb, int x, int y, int w, int h,
                               uint32_t color) {
  draw_rect(bb, x,         y,         w, 1, color); /* top    */
  draw_rect(bb, x,         y + h - 1, w, 1, color); /* bottom */
  draw_rect(bb, x,         y,         1, h, color); /* left   */
  draw_rect(bb, x + w - 1, y,         1, h, color); /* right  */
}

/* draw_rect_blend: alpha-blend a rectangle over the existing pixels.
 *
 * out_channel = (src * alpha + dst * (255 - alpha)) >> 8
 *
 * alpha = 0   → completely transparent (nothing drawn)
 * alpha = 255 → completely opaque (same as draw_rect)
 * alpha = 128 → 50% blend
 *
 * Use for translucent overlays (e.g. the level-complete panel). */
static void draw_rect_blend(GameBackbuffer *bb, int x, int y, int w, int h,
                             uint32_t color, int alpha) {
  uint32_t sr = (color >> 16) & 0xFF;
  uint32_t sg = (color >>  8) & 0xFF;
  uint32_t sb =  color        & 0xFF;
  int x0 = (x < 0)             ? 0          : x;
  int y0 = (y < 0)             ? 0          : y;
  int x1 = (x + w > bb->width) ? bb->width  : x + w;
  int y1 = (y + h > bb->height)? bb->height : y + h;
  for (int py = y0; py < y1; py++) {
    uint32_t *row = bb->pixels + py * bb->width;
    for (int px = x0; px < x1; px++) {
      uint32_t d  = row[px];
      uint32_t dr = (d >> 16) & 0xFF;
      uint32_t dg = (d >>  8) & 0xFF;
      uint32_t db =  d        & 0xFF;
      row[px] = GAME_RGB(
          (sr * alpha + dr * (255 - alpha)) >> 8,
          (sg * alpha + dg * (255 - alpha)) >> 8,
          (sb * alpha + db * (255 - alpha)) >> 8);
    }
  }
}
```

**What's happening:**

- The unsigned cast in `draw_pixel` is a classic C micro-optimisation: one comparison instead of two.  You'll see it everywhere in systems code.
- `draw_rect` clamps to the canvas edge so you never need to check bounds before calling it.  Passing `x = -5, w = 100` is fine — it draws only the visible part.
- `draw_rect_blend` does the standard `src over dst` alpha blend in integer arithmetic (no floats).  Shifting right by 8 is an integer divide by 256 — close enough to 255 for visual purposes and much cheaper.

---

## Step 3: `game_render()` — centralise all drawing

Move the canvas-clear from `main()` into a proper `game_render` function:

```c
/* src/game.c  —  game_render (Lesson 02 version) */

void game_render(const GameState *state, GameBackbuffer *bb) {
  /* Clear canvas to sky-blue each frame.
   * We do this by filling the entire pixel array with the background color.
   * In later lessons this loop gets replaced by a single draw_rect call. */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++)
    bb->pixels[i] = COLOR_BG;

  /* Draw a demo rectangle so we can see the drawing primitives work. */
  draw_rect(bb, 200, 150, 240, 180, GAME_RGB(255, 220, 100));  /* gold fill  */
  draw_rect_outline(bb, 200, 150, 240, 180, COLOR_LINE);        /* dark border */
}
```

Update `game.h` to declare `game_render`:

```c
/* src/game.h  —  function declarations (add) */
void game_render(const GameState *state, GameBackbuffer *backbuffer);
```

Update the main loop in both backends — replace the manual clear loop with:

```c
/* in main(), inside the game loop — both backends */
game_render(&state, &bb);
platform_display_backbuffer(&bb);
```

---

## Step 4: Why the R↔B swap matters (Raylib only)

Here is what happens to a pure-red pixel `GAME_RGB(255, 0, 0) = 0xFFFF0000`:

| Stage | Value | Bytes in memory (little-endian) |
|-------|-------|---------------------------------|
| After `GAME_RGB(255,0,0)` | `0xFFFF0000` | `[0x00, 0x00, 0xFF, 0xFF]` = B,G,R,A |
| Raylib reads as RGBA      | R=0x00, G=0x00, B=0xFF, A=0xFF | Appears **blue** |
| After R↔B swap: `0xFF0000FF` | `[0xFF, 0x00, 0x00, 0xFF]` | R=0xFF → **red** ✓ |

X11 reads XImage bytes natively as BGR — matching our [B,G,R,A] storage — so no swap is needed there.

**Rule:** Test with a bright-red rectangle on both backends to confirm colors are correct before adding game content.

---

## Build and run

```bash
# Raylib
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG -fsanitize=address,undefined \
      -o build/game src/main_raylib.c src/game.c \
      -lraylib -lm -lpthread -ldl
./build/game

# X11
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG -fsanitize=address,undefined \
      -o build/game src/main_x11.c src/game.c \
      -lX11 -lm
./build/game
```

**Expected output:** A sky-blue canvas with a gold rectangle centered on screen with a dark border.

---

## Exercises

1. Call `draw_rect_blend` to draw a semi-transparent black overlay over the whole canvas (use `alpha = 100`).  What does the sky-blue look like at 40% opacity?
2. Write a `draw_circle` function using the same `draw_pixel` helper.  The formula for a filled circle is `dx*dx + dy*dy <= r*r`.
3. Experiment: pass `x = -50` to `draw_rect` with `w = 100`.  Verify only the right 50 pixels are drawn (no crash, no garbage).

---

## What's next

In Lesson 03 we add the full input system — mouse position, left-button state,
and keyboard keys — using the `GameInput` double-buffer pattern.
