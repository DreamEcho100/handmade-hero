# Lesson 06: The Line Bitmap — Collision, Player Drawing, and IS_SOLID

## What we're building

Add a `LineBitmap` — a 640×480 byte array where every non-zero value is solid.
The player can draw lines with the mouse; grains stop when they hit those lines.
Grains also stop each other (grain-grain collision) via a per-frame occupancy bitmap.

## What you'll learn

- The `LineBitmap` dual-purpose encoding (0 = empty, 1 = obstacle/line, 2–5 = baked grain color)
- The `IS_SOLID` macro and why out-of-bounds X is treated as a wall
- Sub-step collision detection revisited — now with a proper solid check
- Bresenham's line algorithm for gapless brush strokes
- Grain-grain collision via an `s_occ[]` occupancy bitmap rebuilt each frame
- Why `s_occ` is `static` (avoids a 300 KB stack allocation every frame)

## Prerequisites

- Lesson 05 complete (GrainPool running, build-dev.sh working)

---

## Step 1: `LineBitmap` type and encoding

```c
/* src/game.h  —  LineBitmap */

/* LineBitmap encodes one byte per canvas pixel:
 *
 *   0        → empty (grain can move here)
 *   1        → solid obstacle or player-drawn line (stamped as 255 in the
 *               brush, obstacles as 1 — both are non-zero → solid)
 *   255      → player-drawn line (same physics as 1)
 *   2..5     → baked settled grain, color = value - 2
 *              (GRAIN_WHITE=0 → stored as 2, GRAIN_RED → 3, etc.)
 *
 * Physics: any non-zero value is solid (IS_SOLID checks != 0).
 * Renderer: value 1 or 255 → COLOR_LINE; value 2..5 → grain color lookup.
 *
 * This single-byte scheme avoids a second array for "what color was this
 * settled grain?" while keeping O(1) lookup per pixel.
 *
 * Memory: 640×480 = 307,200 bytes ≈ 300 KB — fits entirely in L2 cache. */
typedef struct {
  uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;
```

Update `GameState`:

```c
/* src/game.h  —  GameState (add LineBitmap) */
typedef struct {
  int should_quit;
  int mouse_x, mouse_y;
  GrainPool grains;
  LineBitmap lines;      /* ← new: player-drawn + obstacle + baked pixels */
  float spawn_timer;
} GameState;
```

---

## Step 2: The `IS_SOLID` macro

```c
/* src/game.c  —  inside update_grains, before the per-grain loop */

/* Grain occupancy bitmap — rebuilt each frame from current positions.
 * static: avoids a 307 KB stack allocation every call.  Lives in BSS
 * (zero-initialised at program start, no overhead). */
static uint8_t s_occ[CANVAS_W * CANVAS_H];
memset(s_occ, 0, sizeof(s_occ));

/* Pre-populate with current grain positions */
for (int i = 0; i < p->count; i++) {
  if (!p->active[i]) continue;
  int ix = (int)p->x[i], iy = (int)p->y[i];
  if (ix >= 0 && ix < W && iy >= 0 && iy < H)
    s_occ[iy * W + ix] = 1;
}

/* IS_SOLID(xx, yy) — true if (xx, yy) cannot be entered by a grain.
 *
 * Three conditions:
 *   (xx) < 0 || (xx) >= W      → left/right wall (grains wrap Y later but
 *                                  bounce off X walls)
 *   lb->pixels[(yy)*W + (xx)]  → player-drawn line, obstacle, or baked grain
 *   s_occ[(yy)*W + (xx)]       → another grain is already at this pixel
 *
 * Using a macro (not a function) avoids call overhead in the inner loop.
 * These checks run for EVERY sub-step of EVERY active grain. */
#define IS_SOLID(xx, yy)                                      \
  ((xx) < 0 || (xx) >= W                                      \
   || lb->pixels[(yy) * W + (xx)]                             \
   || s_occ[(yy) * W + (xx)])
```

**What's happening:**

- `(xx) < 0 || (xx) >= W` — X walls are solid.  Out-of-bounds Y is handled separately (floor = remove/wrap; ceiling = bounce).
- `lb->pixels` encodes both drawn lines (non-zero) and baked grains (2–5).
- `s_occ` is rebuilt every frame so it always reflects the live grain positions.  A grain is removed from its current `s_occ` cell before it moves so it doesn't block itself.

---

## Step 3: Updated sub-step loop with IS_SOLID checks

```c
/* src/game.c  —  update_grains: revised inner loop per grain */

for (int i = 0; i < p->count; i++) {
  if (!p->active[i]) continue;

  /* Unmark this grain's current position so it doesn't block itself. */
  {
    int cx = (int)p->x[i], cy = (int)p->y[i];
    if (cx >= 0 && cx < W && cy >= 0 && cy < H)
      s_occ[cy * W + cx] = 0;
  }

  /* Apply gravity and clamp */
  p->vy[i] += GRAVITY * dt;
  if (p->vy[i] >  MAX_VY) p->vy[i] =  MAX_VY;
  if (p->vy[i] < -MAX_VY) p->vy[i] = -MAX_VY;

  float total_dx = p->vx[i] * dt;
  float total_dy = p->vy[i] * dt;
  float abs_dx   = total_dx < 0 ? -total_dx : total_dx;
  float abs_dy   = total_dy < 0 ? -total_dy : total_dy;
  int   steps    = (int)(abs_dx + abs_dy) + 1;
  if (steps > 32) steps = 32;
  float sdx = total_dx / (float)steps;
  float sdy = total_dy / (float)steps;

  for (int s = 0; s < steps; s++) {
    float nx = p->x[i] + sdx;
    float ny = p->y[i] + sdy;
    int   ix = (int)nx;
    int   iy = (int)ny;

    /* X walls */
    if (ix < 0)  { p->x[i] = 0.0f;            p->vx[i] = 0.0f; break; }
    if (ix >= W) { p->x[i] = (float)(W - 1);  p->vx[i] = 0.0f; break; }

    /* Ceiling */
    if (iy < 0)  { p->y[i] = 0.0f;  p->vy[i] = 0.0f; break; }

    /* Floor — remove grain */
    if (iy >= H) { p->active[i] = 0; goto next_grain; }

    /* Free pixel — move there */
    if (!IS_SOLID(ix, iy)) {
      p->x[i] = nx;
      p->y[i] = ny;
      continue;
    }

    /* COLLISION with a solid pixel.
     * For now: bounce slightly and stop sub-stepping.
     * Lesson 07 adds the diagonal slide logic. */
    {
      float imp = p->vy[i] < 0 ? -p->vy[i] : p->vy[i];
      p->vy[i] = (imp > GRAIN_BOUNCE_MIN) ? -imp * GRAIN_BOUNCE : 0.0f;
      p->vx[i] = 0.0f;
    }
    break; /* stop sub-stepping after collision */
  }

  /* Re-mark final position in occupancy map */
  {
    int fx = (int)p->x[i], fy = (int)p->y[i];
    if (fx >= 0 && fx < W && fy >= 0 && fy < H)
      s_occ[fy * W + fx] = 1;
  }

next_grain:;
}

#undef IS_SOLID
```

---

## Step 4: Player drawing — Bresenham brush line

```c
/* src/game.c  —  stamp_circle and draw_brush_line */

/* stamp_circle: paint a filled circle of radius r at (cx, cy) in the line bitmap.
 * value 255 = player-drawn (same physics as obstacle value 1). */
static void stamp_circle(LineBitmap *lb, int cx, int cy, int r, uint8_t value) {
  int W = CANVAS_W, H = CANVAS_H;
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r) {
        int px = cx + dx, py = cy + dy;
        if (px >= 0 && px < W && py >= 0 && py < H)
          lb->pixels[py * W + px] = value;
      }
}

/* draw_brush_line: stamp a circle at every pixel along the line from
 * (x0, y0) to (x1, y1) using Bresenham's algorithm.
 *
 * Why Bresenham?  When the mouse moves 20 pixels in one frame, we only
 * receive one MotionNotify event (or one Raylib poll).  Without Bresenham
 * we would stamp a single circle at the endpoint — leaving a 20-pixel gap
 * in the drawn line.  Bresenham fills in every pixel along the path.
 *
 * BRUSH_RADIUS = 2 px gives a thin but visible line. */
#define BRUSH_RADIUS 2

static void draw_brush_line(LineBitmap *lb, int x0, int y0, int x1, int y1,
                             int radius) {
  int dx  = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int dy  = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
  int sx  = (x0 < x1) ? 1 : -1;
  int sy  = (y0 < y1) ? 1 : -1;
  int err = dx + dy;
  int x = x0, y = y0;

  for (;;) {
    stamp_circle(lb, x, y, radius, 255);
    if (x == x1 && y == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x += sx; }
    if (e2 <= dx) { err += dx; y += sy; }
  }
}
```

Wire drawing into `game_update`:

```c
/* src/game.c  —  game_update: add mouse drawing */
void game_update(GameState *state, GameInput *input, float dt) {
  if (dt > 0.1f) dt = 0.1f;
  state->mouse_x = input->mouse.x;
  state->mouse_y = input->mouse.y;
  if (BUTTON_PRESSED(input->escape)) state->should_quit = 1;

  /* Draw brush line while left mouse button is held */
  if (input->mouse.left.ended_down) {
    draw_brush_line(&state->lines,
                    input->mouse.prev_x, input->mouse.prev_y,
                    input->mouse.x,      input->mouse.y,
                    BRUSH_RADIUS);
  }

  spawn_grains(state, dt);
  update_grains(state, dt);
}
```

---

## Step 5: Render the line bitmap

Update `game_render` to draw the `LineBitmap` on top of the background:

```c
/* src/game.c  —  game_render: draw lines and baked grains */
void game_render(const GameState *state, GameBackbuffer *bb) {
  /* Clear canvas */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++)
    bb->pixels[i] = COLOR_BG;

  /* Render LineBitmap pixels (drawn lines + baked grains from Lesson 07) */
  const uint8_t *lp = state->lines.pixels;
  for (int py = 0; py < CANVAS_H; py++) {
    for (int px = 0; px < CANVAS_W; px++) {
      uint8_t v = lp[py * CANVAS_W + px];
      if (v == 0) continue;
      if (v == 1 || v == 255) {
        draw_pixel(bb, px, py, COLOR_LINE);
      } else if (v >= 2 && v < 2 + GRAIN_COLOR_COUNT) {
        draw_pixel(bb, px, py, g_grain_colors[v - 2]);
      }
    }
  }

  /* Render active grains */
  const GrainPool *p = &state->grains;
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;
    draw_pixel(bb, (int)p->x[i], (int)p->y[i],
               g_grain_colors[p->color[i]]);
  }

  /* Cursor */
  draw_circle(bb, state->mouse_x, state->mouse_y, 4, COLOR_LINE);
}
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:** Grains fall from the top.  Hold the left mouse button and drag to draw dark lines — grains pile up on them.  The occupancy bitmap prevents grains from stacking into the same pixel.

---

## Exercises

1. Change `BRUSH_RADIUS` to 0 (single pixel).  Does the brush produce gaps when you move the mouse fast?  Change it back to 2.
2. Add a "clear drawing" button (`C` key) that does `memset(&state->lines, 0, sizeof(state->lines))`.  What happens to grains that were resting on lines?
3. What value would you store for a "GRAIN_RED grain baked at position (x,y)"?  Verify your answer by checking the `v >= 2` render branch above.

---

## What's next

In Lesson 07 we add the diagonal slide logic — when a grain hits a solid surface
it slides to an adjacent free pixel — and the settle-and-bake mechanism that
permanently fixes settled grains into the line bitmap and frees their pool slots.
