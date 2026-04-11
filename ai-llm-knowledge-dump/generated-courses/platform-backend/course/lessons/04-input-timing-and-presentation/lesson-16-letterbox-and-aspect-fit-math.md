# Lesson 16: Letterbox Math and Aspect-Fit Size Calculation

## Frontmatter

- Module: `04-input-timing-and-presentation`
- Lesson: `16`
- Status: Required
- Target files:
  - `src/platform.h`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `platform_compute_letterbox`
  - `platform_compute_aspect_size`
  - `display_backbuffer`

## Observable Outcome

By the end of this lesson, you should be able to compute the scale and centering offsets for a resized window by hand and explain why that math usually does not change the backbuffer itself.

You should also be able to explain why these two helpers are related but different:

- `platform_compute_letterbox(...)`
- `platform_compute_aspect_size(...)`

## Prerequisite Bridge

Lesson 15 explained how one frame fits inside a time budget.

Now Module 04 returns to the visual side of frame trust:

```text
once a correct frame image exists,
how do we fit it into a differently sized window without stretching it incorrectly?
```

That is the job of the shared presentation math in `src/platform.h`.

## Why This Lesson Exists

Window resizing creates two distinct questions that beginners often blur together:

1. where should the current canvas be drawn inside the window?
2. should the canvas itself change size?

Lesson 16 answers only the first question for now.

It teaches the shared aspect-fit math used to place an existing canvas inside a real window without distortion.

Lesson 17 will handle the second question, which is about resize policy.

## Step 1: Start With the Core Goal in Plain Language

Letterboxing is solving a three-part problem:

```text
preserve aspect
fit inside the window
center the result
```

That is it.

It is not backend-specific magic.
It is geometry.

## Step 2: Work One Numeric Example First

Suppose the canvas is `800 x 600` and the window is `1280 x 720`.

Compute the two fit ratios:

$$
s_x = \frac{1280}{800} = 1.6
$$

$$
s_y = \frac{720}{600} = 1.2
$$

To preserve aspect, the scale must be uniform, so we choose the smaller ratio:

$$
\text{scale} = 1.2
$$

That guarantees the whole image fits within both dimensions.

## Step 3: Read `platform_compute_letterbox(...)`

The live helper is:

```c
static inline void platform_compute_letterbox(int win_w, int win_h,
                                              int canvas_w, int canvas_h,
                                              float *scale, int *off_x,
                                              int *off_y) {
  float sx = (float)win_w / (float)canvas_w;
  float sy = (float)win_h / (float)canvas_h;
  *scale = (sx < sy) ? sx : sy;
  *off_x = (int)((win_w - canvas_w * *scale) * 0.5f);
  *off_y = (int)((win_h - canvas_h * *scale) * 0.5f);
}
```

Read that as three sequential steps:

1. compute how much width-fit scaling is available
2. compute how much height-fit scaling is available
3. choose the smaller one, then center the scaled result

## Step 4: Why the Minimum Scale Wins

The important line is:

```c
*scale = (sx < sy) ? sx : sy;
```

Mathematically:

$$
\text{scale} = \min\left(\frac{\text{win}_w}{\text{canvas}_w},
                           \frac{\text{win}_h}{\text{canvas}_h}\right)
$$

Why the minimum?

Because the image must fit in both axes.

If you used the larger ratio, one dimension would overflow the window.

That is the entire reason letterbox scaling chooses the smaller fit.

## Visual: The Postcard Mental Model

```text
the canvas is a rigid postcard
you may scale it uniformly
you may not stretch width and height independently
choose the largest uniform scale that still fits completely
```

This is a good mental model because it makes the `min(...)` rule feel inevitable.

## Step 5: Compute the Offsets After Scale

Once the scale is chosen, leftover space determines centering offsets.

For the `1280 x 720` window example:

$$
\text{scaled width} = 800 \times 1.2 = 960
$$

$$
\text{scaled height} = 600 \times 1.2 = 720
$$

So vertical fit is exact, but horizontal space remains:

$$
1280 - 960 = 320
$$

Centered, that becomes:

```text
off_x = 160
off_y = 0
```

So the image starts `160` pixels from the left edge and leaves `160` pixels on the right.

## Step 6: Read `platform_compute_aspect_size(...)` as a Different Question

The second helper is:

```c
static inline void platform_compute_aspect_size(int win_w, int win_h,
                                                float aspect_w, float aspect_h,
                                                int *out_w, int *out_h) {
  float target_aspect = aspect_w / aspect_h;
  float window_aspect = (float)win_w / (float)win_h;

  if (window_aspect > target_aspect) {
    *out_h = win_h;
    *out_w = (int)(win_h * target_aspect);
  } else {
    *out_w = win_w;
    *out_h = (int)(win_w / target_aspect);
  }
}
```

This helper is not answering “where do I draw the current canvas?”

It is answering:

```text
if I want the canvas itself to resize while preserving a target aspect,
what should the new canvas dimensions be?
```

That is a policy-support helper, not just a placement helper.

## Step 7: Keep the Two Helpers Distinct

Use this memory trick:

```text
platform_compute_letterbox   -> where should the existing canvas be drawn?
platform_compute_aspect_size -> what size should a resized canvas become?
```

They are mathematically related, but operationally different.

This distinction becomes critical in Lesson 17.

## Step 8: See the Shared Math Inside Both Backends

Raylib fixed-mode presentation uses `platform_compute_letterbox(...)` inside `display_backbuffer(...)`:

```c
platform_compute_letterbox(win_w, win_h, backbuffer->width,
                           backbuffer->height, &scale, &ioff_x, &ioff_y);
```

X11 fixed-mode presentation uses the same helper:

```c
platform_compute_letterbox(g_x11.window_w, g_x11.window_h,
                           backbuffer->width, backbuffer->height,
                           &scale, &off_x, &off_y);
```

That is exactly why this math belongs in shared code.

The rendering APIs differ, but the geometry is identical.

## Worked Example: Window Wider Than Canvas Aspect

Canvas: `800 x 600`

Window: `1000 x 600`

Then:

$$
s_x = \frac{1000}{800} = 1.25
$$

$$
s_y = \frac{600}{600} = 1.0
$$

So the chosen scale is `1.0`.

That gives:

```text
dst_w = 800
dst_h = 600
off_x = (1000 - 800) / 2 = 100
off_y = 0
```

So the bars appear on the left and right.

## Step 9: Why Letterboxing Usually Does Not Change the Backbuffer

This is the key architectural point of the lesson.

In fixed-mode presentation, the backbuffer can stay exactly the same size.

The backend only changes:

- the scale applied when presenting it
- the offset used to center it

So black bars usually mean:

```text
presentation placement changed
the backbuffer did not
```

This separation is what keeps the rendering pipeline stable while the real window changes around it.

## Practical Exercises

### Exercise 1: Compute the Letterbox Result

Canvas: `800 x 600`

Window: `1200 x 900`

Compute:

1. chosen scale
2. scaled width and height
3. offsets

Expected result:

```text
scale = 1.5
size  = 1200 x 900
offsets = 0, 0
```

### Exercise 2: Explain the Helper Difference

In one sentence each, explain what `platform_compute_letterbox(...)` and `platform_compute_aspect_size(...)` return conceptually.

Target answers:

```text
letterbox -> presentation scale and offsets for an existing canvas
aspect size -> new canvas dimensions that preserve a target aspect
```

### Exercise 3: Predict the Bars

If a window is taller than the canvas aspect allows, where do the bars appear?

Expected answer:

```text
top and bottom
```

because height becomes the limiting fit axis.

## Common Mistakes

### Mistake 1: Thinking black bars mean the backbuffer resized

Often they only mean presentation placement changed.

### Mistake 2: Mixing up destination placement with canvas resizing

Those are different layers and use different helpers.

### Mistake 3: Treating letterbox math as backend-specific

The math is shared; only the final draw calls differ.

## Troubleshooting Your Understanding

### “I keep mixing up the window size and the canvas size”

Say the question out loud first:

```text
am I placing an existing canvas,
or choosing a new canvas size?
```

That usually tells you which helper belongs.

### “Why compute offsets after scale?”

Because centering depends on the remaining space after the canvas has been scaled to its final drawn size.

## Recap

You now understand the shared presentation math:

1. letterboxing preserves aspect by choosing the smaller fit ratio
2. offsets center the scaled canvas in the real window
3. `platform_compute_letterbox(...)` is about destination placement
4. `platform_compute_aspect_size(...)` is about choosing resized canvas dimensions
5. black bars usually indicate presentation policy, not backbuffer mutation

## Next Lesson

Lesson 17 takes the final step:

```text
when the window changes size,
should the backbuffer stay fixed,
match the window,
or resize while preserving a chosen aspect?
```

That is the scale-mode policy layer built on top of the math from this lesson.
