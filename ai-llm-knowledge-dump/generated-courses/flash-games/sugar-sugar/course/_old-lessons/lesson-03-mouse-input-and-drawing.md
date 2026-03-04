# Lesson 03 — Mouse Input and Drawing

## What you build

A brush tool that lets the player draw solid lines on the canvas by holding and dragging the left mouse button — gapless even at high mouse speed, using the Bresenham line algorithm — and stores those lines in `LineBitmap` for later use as collision geometry.

## Concepts introduced

- `GameButtonState` — the half-transition input model
- `UPDATE_BUTTON` macro
- `BUTTON_PRESSED` / `BUTTON_RELEASED` convenience macros
- `prepare_input_frame()` and why it must run before `platform_get_input()`
- `MouseInput.prev_x` / `prev_y` — snapshot-before-event-loop pattern
- `LineBitmap` — the single-byte collision + render map
- `stamp_circle` — filling a circular region in `LineBitmap`
- Bresenham line algorithm — stepping along the dominant axis
- `draw_brush_line` — Bresenham + `stamp_circle` for thick, gapless strokes

---

## JS → C analogy

| JavaScript                                                   | C equivalent                                                  |
|--------------------------------------------------------------|---------------------------------------------------------------|
| `{ isDown: bool, pressedThisFrame: bool }`                   | `GameButtonState { ended_down, half_transition_count }`       |
| `pressedThisFrame = true` set in `mousedown` handler         | `half_transition_count++` via `UPDATE_BUTTON(btn, 1)`        |
| Clearing `pressedThisFrame = false` at top of rAF callback   | `prepare_input_frame(&input)` at top of game loop            |
| `canvas.addEventListener('mousemove', e => { prevX = x; x = e.clientX; })` | snapshot `prev_x = x` before event loop, update `x` inside it |
| `ctx.beginPath(); ctx.moveTo(px,py); ctx.lineTo(x,y); ctx.stroke()` | `draw_brush_line(&lb, px, py, x, y, BRUSH_RADIUS)`      |
| `imageData.data[y*w+x] = 255` (one channel of a byte map)   | `lb->pixels[y * CANVAS_W + x] = 255`                        |

---

## Step-by-step

### Step 1: Understand `GameButtonState`

The struct is defined in `game.h`:

```c
typedef struct {
  int half_transition_count; /* +1 for every press OR release this frame */
  int ended_down;            /* 1 = currently held, 0 = currently released */
} GameButtonState;
```

A "half-transition" is any change of state — press counts as one, release counts as another. By counting transitions rather than just storing a boolean, we can detect:

- **Held:** `ended_down == 1` (regardless of transitions)
- **Pressed this frame:** `half_transition_count > 0 && ended_down` — button went down at least once and ended down
- **Released this frame:** `half_transition_count > 0 && !ended_down` — button changed state and ended up

The convenience macros:

```c
#define BUTTON_PRESSED(b)  ((b).half_transition_count > 0 && (b).ended_down)
#define BUTTON_RELEASED(b) ((b).half_transition_count > 0 && !(b).ended_down)
```

**Why not just a bool?** On a slow machine or under debugger load, two or more press/release events can arrive in a single frame. A simple bool `wasPressed = true` would lose the intermediate release between them. The half-transition model detects "pressed and released in the same frame" correctly.

### Step 2: `UPDATE_BUTTON` — only count real state changes

The `UPDATE_BUTTON` macro guards against duplicate events:

```c
#define UPDATE_BUTTON(button, is_down)                          \
  do {                                                          \
    if ((button).ended_down != (is_down)) {                     \
      (button).half_transition_count++;                         \
      (button).ended_down = (is_down);                          \
    }                                                           \
  } while (0)
```

If the OS sends an auto-repeat key event (held key fires repeatedly), `is_down` stays `1` while `ended_down` is already `1` — the `if` fails, so `half_transition_count` is NOT incremented. Without this guard, `BUTTON_PRESSED` would trigger every auto-repeat frame even though the key was never released.

**Usage in `main_x11.c`:**

```c
case ButtonPress:
    if (event.xbutton.button == Button1) {
        input->mouse.prev_x = event.xbutton.x;
        input->mouse.prev_y = event.xbutton.y;
        input->mouse.x      = event.xbutton.x;
        input->mouse.y      = event.xbutton.y;
        UPDATE_BUTTON(input->mouse.left, 1);
    }
    break;

case ButtonRelease:
    if (event.xbutton.button == Button1)
        UPDATE_BUTTON(input->mouse.left, 0);
    break;
```

### Step 3: `prepare_input_frame()` — reset counters every frame

```c
void prepare_input_frame(GameInput *input) {
  /* Reset the "how many times did this change?" counter each frame.
   * We do NOT reset ended_down — that would lose the current hold state. */
  input->mouse.left.half_transition_count  = 0;
  input->mouse.right.half_transition_count = 0;
  input->escape.half_transition_count      = 0;
  input->reset.half_transition_count       = 0;
  input->gravity.half_transition_count     = 0;
}
```

This **must** be called **before** `platform_get_input()` each frame. The game loop sequence is:

```c
prepare_input_frame(&input);   /* 1. clear this-frame counters */
platform_get_input(&input);    /* 2. fill counters from OS events */
game_update(&state, &input, delta_time); /* 3. consume counters */
```

If you call `prepare_input_frame` *after* `platform_get_input`, you erase the events you just collected before `game_update` can read them. If you forget `prepare_input_frame` entirely, `half_transition_count` accumulates across frames — `BUTTON_PRESSED` will be true forever after the first click.

**JS analogy:**

```js
// Start of requestAnimationFrame callback:
input.pressedThisFrame = false;   // ← this IS prepare_input_frame
input.releasedThisFrame = false;
// ... then process events ...
update(input);
```

### Step 4: The `prev_x` / `prev_y` snapshot pattern

`MouseInput` carries both current and previous position:

```c
typedef struct {
  int x, y;           /* current pixel position this frame  */
  int prev_x, prev_y; /* position last frame (for draw lines) */
  GameButtonState left;
  GameButtonState right;
} MouseInput;
```

The critical detail is **when** `prev_x/prev_y` is snapped. In `main_x11.c`:

```c
void platform_get_input(GameInput *input) {
    /* ---- Save start-of-frame mouse position BEFORE processing events ----
     *
     * X11 can queue MANY MotionNotify events between frames (e.g. 10+
     * events when the mouse moves fast).  If we update prev_x inside the
     * event loop, by the time game_update runs, prev_x/y only holds the
     * position from the second-to-last event — all earlier segments of the
     * mouse path are lost.  draw_brush_line(prev_x, prev_y, x, y) then
     * draws only the tiny last segment, leaving visible gaps.
     *
     * Fix: snapshot the position once here, before the event loop. */
    input->mouse.prev_x = input->mouse.x;
    input->mouse.prev_y = input->mouse.y;

    while (XPending(g_display)) {
        XEvent event;
        XNextEvent(g_display, &event);
        switch (event.type) {
        case MotionNotify:
            /* Update current position only — prev is already saved above. */
            input->mouse.x = event.xmotion.x;
            input->mouse.y = event.xmotion.y;
            break;
        /* ... */
        }
    }
}
```

After this function returns, `prev_x/y` = start-of-frame position and `x/y` = end-of-frame position, spanning the entire mouse path the user traveled this frame. `draw_brush_line` then interpolates the full path in one Bresenham call, covering every pixel without gaps.

### Step 5: `LineBitmap` — the single-byte pixel map

`LineBitmap` is defined in `game.h`:

```c
typedef struct {
  uint8_t pixels[CANVAS_W * CANVAS_H];
} LineBitmap;
```

Each byte encodes one pixel of the collision + render layer:

| Value   | Meaning                                                     |
|---------|-------------------------------------------------------------|
| `0`     | Empty — grains pass through freely                         |
| `1`     | Obstacle / cup wall (stamped at level load)                |
| `255`   | Player-drawn line (stamped by the brush)                   |
| `2..5`  | Baked settled grain, color = `value - 2` (GRAIN_WHITE=2, etc.) |

The physics engine checks `!= 0` for solidity:

```c
#define IS_SOLID(xx, yy) \
  ((xx) < 0 || (xx) >= W || lb->pixels[(yy) * W + (xx)] || s_occ[(yy) * W + (xx)])
```

The renderer uses the value to choose the draw color:

```c
/* value 1 or 255 → draw as line color */
/* value 2..5    → draw as grain color (value - 2) */
uint8_t v = lb->pixels[iy * W + ix];
uint32_t c = (v == 1 || v == 255)
    ? COLOR_LINE
    : g_grain_colors[v - 2];
```

At `640 × 480 = 307,200 bytes ≈ 300 KB`, the entire `LineBitmap` fits in L2 cache, making the per-grain solidity check very fast.

### Step 6: `stamp_circle` — painting to `LineBitmap`

```c
static void stamp_circle(LineBitmap *lb, int cx, int cy, int r, uint8_t val) {
  int W = CANVAS_W, H = CANVAS_H;
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        int px = cx + dx, py = cy + dy;
        if (px >= 0 && px < W && py >= 0 && py < H)
          lb->pixels[py * W + px] = val;
      }
    }
  }
}
```

Called with `val = 255` when the player draws a line, and with `val = 1` when stamping obstacles or cup walls at level load.

### Step 7: Bresenham line algorithm — the core idea

When the player drags the mouse fast, consecutive frames may report positions tens of pixels apart. Drawing only a single circle at the endpoint leaves a visible gap. The Bresenham algorithm walks every pixel along the straight line from `(x0,y0)` to `(x1,y1)` and stamps a circle at each step.

**How it works:**

Bresenham's algorithm uses only integer arithmetic. It maintains an `err` accumulator that tracks how far the actual line has drifted from the pixel grid. Each step moves along the **dominant axis** (the one with the larger `|delta|`) and conditionally moves along the secondary axis when `err` crosses zero.

```
dx = |x1 - x0|       (horizontal span, always positive)
dy = |y1 - y0|       (vertical span, always positive, negated in err for direction)
err = dx - dy        (initial error term)

loop:
  stamp at (x, y)
  if x == x1 && y == y1: done
  e2 = 2 * err
  if e2 >= dy:  err += dy; x += sx   (step horizontal)
  if e2 <= dx:  err += dx; y += sy   (step vertical)
```

`sx` / `sy` are the step directions: `+1` or `-1` depending on whether we're going right/left and down/up.

The algorithm guarantees that no pixel along the mathematical line is skipped — every pixel the line passes through receives a `stamp_circle` call.

### Step 8: `draw_brush_line` — the full implementation

```c
static void draw_brush_line(LineBitmap *lb, int x0, int y0, int x1, int y1,
                            int radius) {
  /* Walk every pixel along the line using the Bresenham algorithm,
   * then stamp a filled circle at each point.
   * This ensures thick, gapless lines even when the mouse moves fast. */
  int dx  = (x1 > x0 ? x1 - x0 : x0 - x1);
  int dy  = -(y1 > y0 ? y1 - y0 : y0 - y1);
  int sx  = (x0 < x1 ? 1 : -1);
  int sy  = (y0 < y1 ? 1 : -1);
  int err = dx + dy;
  int x = x0, y = y0;

  for (;;) {
    stamp_circle(lb, x, y, radius, 255);
    if (x == x1 && y == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x += sx; }
    if (e2 <= dx) { err += dx; y += sy; }
  }
}
```

Note `dy` is **negated** at initialisation. This is the standard Bresenham formulation — `dy` carries the vertical error contribution as a negative number so the `e2 >= dy` check works for both octants.

**Brush radius:** `BRUSH_RADIUS` is defined in `game.c` as `2` pixels. `stamp_circle` is called with that radius, producing a 5-pixel-diameter circular brush head at every step.

### Step 9: Where drawing is triggered in `game_update`

In `game.c`, inside `update_playing`:

```c
/* Draw line when left mouse button is held */
if (input->mouse.left.ended_down) {
    draw_brush_line(&state->lines,
                    input->mouse.prev_x, input->mouse.prev_y,
                    input->mouse.x,      input->mouse.y,
                    BRUSH_RADIUS);
}
```

And equivalently in `update_freeplay`. The brush draws whenever the button is held — no explicit "first frame" check needed. On the very first frame of a click, `prev_x == x` and `prev_y == y` (they were snapped equal just before the click event), so `draw_brush_line` stamps a single circle at the click point.

### Step 10: Verify the brush in action

After building, click and drag the mouse over the canvas. You should see:
- A dark (`COLOR_LINE`) stroke appear on the sky-blue canvas.
- The stroke remains gapless even when dragging quickly.
- Grains spawned by the emitter stack on top of the drawn lines.

The `LineBitmap` is rendered in `render_playing` by iterating every non-zero byte and writing either `COLOR_LINE` (value 1 or 255) or a grain color (value 2–5) to the backbuffer.

---

## Common mistakes to prevent

1. **Calling `prepare_input_frame` after `platform_get_input`.**
   This clears the counters you just collected, making every button appear unpressed. Always call `prepare_input_frame` first.

2. **Updating `prev_x` inside the event loop.**
   If you do `prev_x = x; x = event.x;` inside the `MotionNotify` handler, `prev_x` ends up at the second-to-last event position instead of the start-of-frame position. Fast mouse moves leave a gap at the end of the stroke.

3. **Using `BUTTON_PRESSED` for a held-down drawing action.**
   `BUTTON_PRESSED` is true only on the frame the button goes down. Use `ended_down` for actions that should repeat while held:
   ```c
   if (input->mouse.left.ended_down) { /* draw each frame */ }
   ```

4. **Forgetting to negate `dy` in Bresenham.**
   The algorithm requires `dy = -(abs(y1-y0))`. Without the negation, the `e2 >= dy` and `e2 <= dx` checks use wrong signs and the algorithm draws diagonal lines only in the first octant, producing staircase artifacts in all other directions.

5. **Writing to `LineBitmap` from inside `game_render`.**
   `game_render` takes `const GameState *state`. Writing to `state->lines.pixels` inside render would require removing `const` and would break the update/render separation. Always mutate `LineBitmap` in `game_update`.

6. **Using `BRUSH_RADIUS = 0`.**
   `stamp_circle` with `r = 0` only writes the center pixel. While technically correct, it creates very thin lines that are hard to use as guides and nearly invisible on a 640×480 canvas. The game uses `BRUSH_RADIUS = 2` (5-pixel diameter).

---

## What to run

```bash
./build-dev.sh
./sugar_x11
```

Navigate to any level (press Enter on the title screen). Click and drag on the canvas to draw lines. Build some ramps and watch the sugar grains slide along them. Test fast mouse moves — strokes must be gapless.

```bash
./build-dev.sh -d
./sugar_x11_dbg
```

Run with the debug build to verify that `stamp_circle` never writes out-of-bounds (AddressSanitizer will flag it immediately if it does).

---

## Summary

Input in this engine is built on the half-transition model (`GameButtonState`), which counts state changes within a frame rather than just storing a binary "is held" flag — this correctly handles press-and-release within a single frame and prevents auto-repeat from misfiring. `prepare_input_frame()` must run before `platform_get_input()` every frame to reset those per-frame counters. The most subtle bug in drawing is the `prev_x` snapshot: it must be saved once before the event loop so that Bresenham's algorithm receives the full start-to-end mouse path rather than just the last tiny segment. `draw_brush_line` combines Bresenham (gapless integer line walk) with `stamp_circle` (circular brush head) to write `255` into `LineBitmap` — the same array that the physics engine reads as collision geometry every frame.
