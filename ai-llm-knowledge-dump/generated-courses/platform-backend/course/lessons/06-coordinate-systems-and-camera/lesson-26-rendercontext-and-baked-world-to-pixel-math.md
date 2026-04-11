# Lesson 26: `RenderContext`, Camera Bake-In, and Branch-Free Conversion Math

## Frontmatter

- Module: `06-coordinate-systems-and-camera`
- Lesson: `26`
- Status: Required
- Target files:
  - `src/utils/render.h`
  - `src/utils/base.h`
- Target symbols:
  - `RenderContext`
  - `make_render_context`
  - `world_to_px_x`
  - `world_to_px_y`
  - `x_offset`
  - `y_offset`
  - `x_sign`
  - `y_sign`
  - `rect_top_wh_mul`
  - `rect_left_ww_mul`
  - `text_scale`
  - `line_height`
  - `px_w`
  - `px_h`

## Observable Outcome

By the end of this lesson, you should be able to explain how one `Backbuffer` plus one `GameWorldConfig` becomes a frame-local package of scale, offsets, signs, rectangle-edge flags, and text metrics.

You should also be able to explain why the runtime pays origin/camera branching once in `make_render_context(...)` instead of at every draw site.

## Prerequisite Bridge

Lesson 25 reduced world convention into origin and sign meaning.

The next question is:

```text
how do we turn that meaning into concrete numbers the hot draw path can spend quickly?
```

That is the job of `RenderContext`.

## Why This Lesson Exists

Draw helpers should not keep asking questions like:

- which origin mode is active?
- should `y` be flipped?
- what is the camera zoom right now?
- how many pixels is one world unit this frame?

If every draw site re-solved those questions, the code would become repetitive and branch-heavy.

The course chooses a better boundary:

```text
pay the policy cost once per frame
store the answers in RenderContext
let draw helpers spend those answers cheaply
```

## Step 1: Read `RenderContext` as Precomputed Render Math

In `src/utils/render.h`:

```c
typedef struct RenderContext {
  float world_to_px_x;
  float world_to_px_y;
  float x_offset;
  float y_offset;
  float x_sign;
  float y_sign;
  float rect_top_wh_mul;
  float rect_left_ww_mul;
  float text_scale;
  float line_height;
  float px_w;
  float px_h;
} RenderContext;
```

This is not scene state and it is not backbuffer storage.

It is one frame's baked world-to-pixel conversion package.

## Visual: Why the Context Exists

```text
Backbuffer + GameWorldConfig
          |
          v
make_render_context()
          |
          v
RenderContext
  scale + offsets + signs + rect flags + text metrics
          |
          v
world_x / world_y / world_rect_px_x / world_rect_px_y
          |
          v
draw_rect / draw_text
```

That is the boundary the rest of Module 06 is built around.

## Step 2: Start With the Scale Computation

At the top of `make_render_context(...)`:

```c
ctx.world_to_px_x = (float)bb->width / GAME_UNITS_W * cfg.camera_zoom;
ctx.world_to_px_y = (float)bb->height / GAME_UNITS_H * cfg.camera_zoom;
```

These fields answer one important question:

```text
how many pixels should one world unit occupy right now?
```

At the default `800 x 600` backbuffer with `16 x 12` world units and `zoom = 1.0`, both axes produce `50 px/unit`.

## Worked Example: Zoom Changes the Coefficients, Not the Object

At base scale:

```text
1 world unit = 50 px
```

If `camera_zoom = 2.0`, then:

```text
1 world unit = 100 px
```

The world object did not change.

The conversion coefficients changed.

That is a key camera lesson.

## Step 3: Notice the Context Also Stores Pixel Dimensions

The code also records:

```c
ctx.px_w = (float)bb->width;
ctx.px_h = (float)bb->height;
```

These are not for world conversion itself.

They exist so later helpers like visibility tests can know the viewport bounds without needing extra arguments.

This is a good sign that the context is designed to support more than one helper family.

## Step 4: Read the Origin Switch as a Once-Per-Frame Decision

The large switch inside `make_render_context(...)` chooses:

- offsets
- signs
- rectangle-edge multipliers

once, then stores the answers as floats.

That is the whole branch-elimination strategy.

## Step 5: Walk the Default `LEFT_BOTTOM` Case Carefully

The live code sets:

```c
ctx.x_offset = 0.0f;
ctx.y_offset = GAME_UNITS_H;
ctx.x_sign = 1.0f;
ctx.y_sign = -1.0f;
ctx.rect_top_wh_mul = 1.0f;
ctx.rect_left_ww_mul = 0.0f;
```

Each field carries real meaning.

### `x_offset = 0`

World `x = 0` starts at the left edge.

### `y_offset = GAME_UNITS_H`

The y-up world must be flipped into top-down pixel space.

### `x_sign = +1`

Positive `x` moves right.

### `y_sign = -1`

Positive world `y` moves up, so the pixel formula must invert it.

### `rect_top_wh_mul = 1`

For y-up worlds, the screen-top edge of a rectangle comes from `wy + wh`.

### `rect_left_ww_mul = 0`

For left-to-right worlds, the screen-left edge comes directly from `wx`.

## Step 6: Understand the Rectangle Multipliers

`rect_top_wh_mul` and `rect_left_ww_mul` look small, but they are one of the most important design tricks in the file.

They let later rectangle helpers decide which world edge becomes the screen top or left edge using arithmetic instead of branches.

### Y example

```text
y-up   -> top edge = wy + wh
y-down -> top edge = wy
```

### X example

```text
LTR -> left edge = wx
RTL -> left edge = wx + ww
```

That is what those 0/1 multipliers encode.

## Step 7: Camera Pan Is Baked Into the Offsets

Later in `make_render_context(...)`:

```c
ctx.x_offset -= cfg.camera_x;
ctx.y_offset -= cfg.camera_y * ctx.y_sign;
```

This is where the current view state enters the baked conversion.

The important lesson is:

```text
the camera is not applied later as a separate draw-time hack
it is folded into the conversion coefficients up front
```

That is why later helper calls can remain simple.

## Worked Example: Why the Y Camera Formula Uses `y_sign`

Different origins disagree about whether positive world `y` goes up or down.

So the camera adjustment cannot treat both the same way.

By multiplying with `ctx.y_sign`, the bake step absorbs that convention difference once.

That is exactly the kind of policy work `RenderContext` is meant to hide from later helpers.

## Step 8: Text Metrics Are Also Baked Here

The context also computes:

```c
ctx.text_scale
ctx.line_height
```

The live code derives text scale from backbuffer width relative to the base world-size ratio, then uses `GLYPH_H` to compute line height.

This means text layout helpers later do not need to re-solve font scaling every time.

That is why `TextCursor` can stay tiny in the next lesson.

## Step 9: The Context Is Frame-Local on Purpose

`RenderContext` should be understood as:

```text
valid for this backbuffer shape
valid for this world config
valid for this camera state
valid for this frame
```

If camera or resize policy changes, you rebuild the context next frame.

That is the right scope for this kind of precomputed math.

## Practical Exercises

### Exercise 1: Explain the Bake Step

Why does the runtime build a `RenderContext` once instead of branching on origin inside every draw helper?

Expected answer:

```text
so origin, sign, camera, and rectangle-edge policy are paid once per frame and the hot path can use precomputed floats
```

### Exercise 2: Compute a Zoomed Scale

At base scale, the world uses `50 px/unit`.

What happens when `camera_zoom = 0.5`?

Expected answer:

```text
the scale becomes 25 px/unit, so more world units fit on screen
```

### Exercise 3: Explain the Rectangle Flags

Why are `rect_top_wh_mul` and `rect_left_ww_mul` stored in the context instead of recomputed at every rectangle draw site?

Expected answer:

```text
because they encode the origin-dependent edge choice once so rectangle helpers can stay arithmetic-only later
```

## Common Mistakes

### Mistake 1: Thinking `RenderContext` is another world-state struct

It is not persistent game state. It is precomputed conversion data for one frame.

### Mistake 2: Thinking zoom changes object sizes directly

Zoom changes the world-to-pixel coefficients. The object is still the same size in world units.

### Mistake 3: Ignoring the rectangle-edge multipliers because they look minor

They are one of the main reasons rectangle conversion can remain branch-free.

## Troubleshooting Your Understanding

### “I understand the struct but not why it helps”

Ask yourself what later draw helpers would need to know if the context did not exist. The answer is “too many policy facts per call.”

### “Why does text-related data live in the same context?”

Because text placement is also a function of the current backbuffer and world-to-pixel scale policy, and the runtime wants one frame-local package for render math.

## Recap

You now understand the runtime's bake step:

1. `RenderContext` stores one frame's conversion coefficients and layout facts
2. `make_render_context(...)` computes scale, offsets, signs, rectangle-edge flags, and text metrics
3. camera state is folded into the baked offsets
4. later helpers can use the context without repeatedly branching on origin or camera policy

## Next Lesson

Lesson 27 shows the first major payoff of this design for text: convert one anchor once, then flow many lines in pixel space with a tiny `TextCursor` instead of rerunning world math for every line.
