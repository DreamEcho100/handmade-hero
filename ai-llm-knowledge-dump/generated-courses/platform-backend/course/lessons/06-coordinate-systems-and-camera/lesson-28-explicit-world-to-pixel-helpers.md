# Lesson 28: Explicit World-to-Pixel Helpers and Visible Conversion Boundaries

## Frontmatter

- Module: `06-coordinate-systems-and-camera`
- Lesson: `28`
- Status: Required
- Target files:
  - `src/utils/render_explicit.h`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
- Target symbols:
  - `world_x`
  - `world_y`
  - `world_w`
  - `world_h`
  - `world_rect_px_x`
  - `world_rect_px_y`
  - `game_draw_world_label`

## Observable Outcome

By the end of this lesson, you should be able to look at a draw site and explain exactly where world-space meaning ends and pixel-space drawing begins.

You should also be able to explain why rectangle helpers need more than point helpers when coordinate conventions can vary.

## Prerequisite Bridge

Lesson 27 showed how text layout spends render-context math once and then flows in pixels.

Lesson 28 goes back to world drawing and asks:

```text
how should game code spend the baked RenderContext at ordinary draw sites?
```

The course chooses explicit helpers instead of hiding the conversion boundary entirely.

## Why This Lesson Exists

The course wants draw sites to remain readable about coordinate meaning.

Instead of burying world-to-pixel conversion inside a giant abstraction, it uses small explicit helpers so the code can still say:

```text
I am converting world position here
I am converting world size here
```

That is a teaching choice and an architectural choice.

## Step 1: Read the Axis Helpers First

In `src/utils/render_explicit.h`:

```c
static inline float world_x(const RenderContext *ctx, float wx)
static inline float world_y(const RenderContext *ctx, float wy)
static inline float world_w(const RenderContext *ctx, float ww)
static inline float world_h(const RenderContext *ctx, float wh)
```

These helpers deliberately split two different conversion jobs.

### Position helpers

- `world_x(...)`
- `world_y(...)`

These use offsets and signs because position depends on origin and camera.

### Size helpers

- `world_w(...)`
- `world_h(...)`

These only scale by pixels-per-unit because size does not care where zero lives.

That is a very clean separation.

## Step 2: Why Rectangle Helpers Need Extra Care

A point helper only answers:

```text
where does this world coordinate land in pixels?
```

A rectangle helper must answer more:

```text
which world edge becomes the screen-left edge?
which world edge becomes the screen-top edge?
what are the pixel width and height?
```

That is why the file also defines:

```c
world_rect_px_x(ctx, wx, ww)
world_rect_px_y(ctx, wy, wh)
```

They are not redundant with `world_x(...)` and `world_y(...)`.

They solve a different problem.

## Step 3: Read `world_rect_px_x(...)` as “Find the Screen-Left Edge”

The live helper is:

```c
static inline float world_rect_px_x(const RenderContext *ctx,
                                    float wx, float ww) {
  return (ctx->x_offset + ctx->x_sign * (wx + ww * ctx->rect_left_ww_mul))
       * ctx->world_to_px_x;
}
```

This uses `ctx->rect_left_ww_mul` from the baked context.

So it can encode the convention-dependent edge choice without branching.

### LTR case

```text
rect_left_ww_mul = 0
left edge uses wx
```

### RTL case

```text
rect_left_ww_mul = 1
left edge uses wx + ww
```

That is the direct payoff from Lesson 26's bake step.

## Step 4: Read `world_rect_px_y(...)` as “Find the Screen-Top Edge”

The y helper is:

```c
static inline float world_rect_px_y(const RenderContext *ctx,
                                    float wy, float wh) {
  return (ctx->y_offset + ctx->y_sign * (wy + wh * ctx->rect_top_wh_mul))
       * ctx->world_to_px_y;
}
```

This uses `ctx->rect_top_wh_mul`.

### Y-up case

```text
rect_top_wh_mul = 1
top edge uses wy + wh
```

### Y-down case

```text
rect_top_wh_mul = 0
top edge uses wy
```

This is why rectangle helpers need width or height arguments in addition to position.

## Worked Example: Why `world_rect_px_y(...)` Needs Height

In a y-up world, the screen-top edge of a rectangle is not based on `wy` alone.

It is based on:

```text
wy + wh
```

So the helper cannot possibly return the correct top pixel coordinate unless it knows both the rectangle's world `y` and its world height.

That is the whole reason a point helper is not enough.

## Step 5: Watch the Helpers Pay Off in the Live Runtime

In `src/game/main.c`, world rectangles are drawn like this:

```c
draw_rect(backbuffer, world_rect_px_x(ctx, bar->x, bar->w),
          world_rect_px_y(ctx, bar->y, bar->h), world_w(ctx, bar->w),
          world_h(ctx, bar->h), ...);
```

This is very readable.

At a glance you can see:

```text
world position -> world_rect_px_x / world_rect_px_y
world size     -> world_w / world_h
```

That is exactly what the explicit-helper style is designed to preserve.

## Step 6: Use `game_draw_world_label(...)` as the Smallest Useful Example

In `src/game/main.c`:

```c
static void game_draw_world_label(Backbuffer *backbuffer,
                                  const RenderContext *ctx,
                                  float wx, float wy,
                                  const char *text, ... ) {
  draw_text(backbuffer, world_x(ctx, wx), world_y(ctx, wy),
            (int)ctx->text_scale, text, ...);
}
```

This helper is a perfect teaching example because it is so small.

It does not hide the conversion boundary.

It just packages a common pattern:

```text
world point -> pixel point -> draw text
```

## Step 7: `demo_explicit.c` Shows the Teaching Style Most Directly

The retained `demo_explicit.c` path still demonstrates the explicit-helper philosophy very cleanly.

Its draw sites make the conversion boundary visible, and its culling examples later use the same helpers in a smaller context than the full runtime scenes.

That file is worth keeping in mind as the stripped-down demonstration of this style.

## Visual: Position vs Rectangle Conversion

```text
point:
  world_x / world_y
  -> one pixel location

rectangle:
  world_rect_px_x / world_rect_px_y + world_w / world_h
  -> top-left pixel corner + pixel size
```

That is the distinction you want to keep stable.

## Practical Exercises

### Exercise 1: Explain the Split

Why do `world_w(...)` and `world_h(...)` not need offsets or signs the way `world_x(...)` and `world_y(...)` do?

Expected answer:

```text
because size depends only on scale, while position depends on origin and camera-relative offsets too
```

### Exercise 2: Explain the Rectangle Helpers

Why are `world_rect_px_x(...)` and `world_rect_px_y(...)` not redundant with `world_x(...)` and `world_y(...)`?

Expected answer:

```text
because rectangles need the correct screen-left and screen-top edges, which depend on width/height and the active coordinate convention
```

### Exercise 3: Read a Draw Site

Given:

```c
draw_rect(backbuffer, world_rect_px_x(ctx, x, w),
          world_rect_px_y(ctx, y, h),
          world_w(ctx, w), world_h(ctx, h), ...);
```

Say in plain language what each helper contributes.

## Common Mistakes

### Mistake 1: Thinking `world_x(...)` and `world_rect_px_x(...)` do the same job

They do not. One converts a point, the other chooses a rectangle edge.

### Mistake 2: Forgetting that size conversion is simpler than position conversion

Size only scales. Position must also honor origin and camera policy.

### Mistake 3: Wanting to hide the conversion boundary completely

For this course, visible boundaries are useful because they keep coordinate meaning readable at the call site.

## Troubleshooting Your Understanding

### “Why not just return a whole rectangle struct from one helper?”

That would work technically, but it would hide the separate ideas of position and size conversion that the course is trying to teach explicitly.

### “Why does `game_draw_world_label(...)` still matter if it is tiny?”

Because tiny helpers often show the intended architecture more clearly than large render functions.

## Recap

You now understand the explicit-helper boundary:

1. point helpers convert world positions to pixels
2. size helpers convert world extents to pixel extents
3. rectangle-edge helpers handle convention-dependent top/left edge selection
4. the live draw sites stay readable because they show where world meaning is spent

## Next Lesson

Lesson 29 closes the coordinate module by connecting these helpers back to scene-owned camera state, world-vs-HUD context creation, and visibility culling so the runtime can decide when to move, when to pin, and when to skip drawing entirely.
