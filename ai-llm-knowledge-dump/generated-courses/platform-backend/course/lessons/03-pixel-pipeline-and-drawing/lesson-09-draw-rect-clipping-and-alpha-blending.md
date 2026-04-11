# Lesson 09: `draw_rect`, Clipping, and Alpha Blending

## Frontmatter

- Module: `03-pixel-pipeline-and-drawing`
- Lesson: `09`
- Status: Required
- Target files:
  - `src/utils/draw-shapes.c`
  - `src/utils/draw-shapes.h`
  - `src/utils/backbuffer.h`
- Target symbols:
  - `draw_rect`
  - `CLAMP`
  - `MAX`
  - `MIN`
  - `Backbuffer.pitch`

## Observable Outcome

By the end of this lesson, you should be able to explain exactly how `draw_rect(...)` turns a floating-point rectangle request into correctly clipped pixels in the backbuffer.

You should be able to explain all three rendering paths without guessing:

- fully transparent rectangle -> skip
- fully opaque rectangle -> direct write fast path
- partially transparent rectangle -> per-pixel blend against the destination pixel

## Prerequisite Bridge

Lesson 08 showed how a finished backbuffer image reaches the window.

That raises the next necessary question:

```text
what code actually fills the backbuffer before presentation?
```

The first full answer is `draw_rect(...)`.

Once this function is clear, later glyph and text rendering stop feeling like special-case systems.

## Why This Lesson Exists

`draw_rect(...)` is the first real raster primitive in the course.

That makes it important far beyond rectangles.

This one function teaches almost every core rule of the software renderer:

1. float geometry becomes integer pixel bounds
2. clipping keeps writes inside the backbuffer
3. `pitch` controls row stepping
4. fully opaque drawing can skip destination reads
5. partial alpha requires reading, blending, and repacking the old pixel

If this function is solid in your head, the rest of Module 03 becomes much easier.

## Step 1: Start With the Live Signature

The primitive is:

```c
void draw_rect(Backbuffer *backbuffer,
               float x, float y, float w, float h,
               float r, float g, float b, float a)
```

That signature already teaches three design choices.

### Choice 1: The destination is plain CPU image memory

The target is a `Backbuffer *`, not a platform API object.

So this function knows nothing about Raylib or X11.

### Choice 2: Geometry is expressed in floats

Call sites can stay in float space and let the rasterizer handle the final truncation to integer pixel bounds.

### Choice 3: Colors arrive as normalized floats

Call sites think in `[0.0f, 1.0f]` per channel.

The rasterizer converts that to 8-bit packed color internally.

## Step 2: Reject No-Work Cases Early

The first lines are:

```c
if (!backbuffer || !backbuffer->pixels) return;
if (a <= 0.0f) return;
```

That means:

```text
no valid target memory -> do nothing
fully transparent source -> do nothing
```

This is both defensive and fast.

If a rectangle cannot affect the final image, the function exits immediately.

## Step 3: Clamp Incoming Color Once

The next block is:

```c
float clamped_r = CLAMP(r, 0.0f, 1.0f);
float clamped_g = CLAMP(g, 0.0f, 1.0f);
float clamped_b = CLAMP(b, 0.0f, 1.0f);
float clamped_a = CLAMP(a, 0.0f, 1.0f);
```

Why clamp here?

Because the rasterizer should not assume perfect inputs.

If a caller passes `1.2f` or `-0.3f`, the rasterizer normalizes the values once and then performs the rest of the math on valid channels.

That keeps later channel conversion predictable.

## Step 4: Convert Float Geometry Into a Clip Rectangle

The core bounds calculation is:

```c
int x0 = MAX((int)x, 0);
int y0 = MAX((int)y, 0);
int x1 = MIN((int)(x + w), backbuffer->width);
int y1 = MIN((int)(y + h), backbuffer->height);

if (x0 >= x1 || y0 >= y1) return;
```

Read that as a three-step policy.

### Step A: Compute the raw integer edges

```text
left   = (int)x
top    = (int)y
right  = (int)(x + w)
bottom = (int)(y + h)
```

### Step B: Clamp those edges against the backbuffer bounds

```text
left and top cannot go below 0
right cannot exceed width
bottom cannot exceed height
```

### Step C: If the visible area is empty, stop

That final zero-area test keeps the loops simple and safe.

## Worked Trace: Exact Clipping

Use this rectangle request:

```text
x = -3
y = 2
w = 8
h = 4
```

Assume the backbuffer is `10 x 10`.

### Raw edges

```text
left   = -3
top    = 2
right  = -3 + 8 = 5
bottom = 2 + 4 = 6
```

### Clamped edges

```text
x0 = MAX(-3, 0) = 0
y0 = MAX(2, 0)  = 2
x1 = MIN(5, 10) = 5
y1 = MIN(6, 10) = 6
```

So the loops cover:

```text
rows 2, 3, 4, 5
cols 0, 1, 2, 3, 4
```

That is the clipping habit you want to keep for the whole course:

```text
compute raw edges
clamp once
then trust the loops
```

## Visual: A Partially Off-Screen Rectangle

```text
screen x coordinates:

... -3 -2 -1 | 0 1 2 3 4 5 ...
             |
requested:   [########]
visible:         [#####]
```

The rasterizer does not try to draw the off-screen part and hope for the best.

It computes the visible bounds first.

## Step 5: Convert `pitch` Into a Pixel Stride Once

The function computes:

```c
int stride = backbuffer->pitch / backbuffer->bytes_per_pixel;
```

This matters because `pitch` is stored in bytes but `pixels` is a `uint32_t *`.

So the function converts once into “how many pixels you move to reach the next row” and then uses that stride throughout the loops.

That is why later row starts look like:

```c
uint32_t *dst = backbuffer->pixels + row * stride + x0;
```

## Step 6: Opaque Drawing Uses the Fast Path

If alpha is fully opaque, the function takes this branch:

```c
if (clamped_a >= 1.0f) {
  uint32_t color = (uint32_t)(clamped_r * 255.0f)
                 | ((uint32_t)(clamped_g * 255.0f) << 8)
                 | ((uint32_t)(clamped_b * 255.0f) << 16)
                 | 0xFF000000u;

  for (int row = y0; row < y1; row++) {
    uint32_t *dst = backbuffer->pixels + row * stride + x0;
    for (int col = x0; col < x1; col++) {
      *dst++ = color;
    }
  }
}
```

### Why this is the fast path

An opaque rectangle does not care what was already in the destination pixel.

So the rasterizer can:

1. pack the source color once
2. walk each row with a pointer
3. write directly

No destination read is needed.
No blend math is needed.

## Step 7: Partial Alpha Uses the Blend Path

If alpha is between `0.0f` and `1.0f`, the function does this setup:

```c
int src_a = (int)(clamped_a * 255.0f);
int inv_alpha = 255 - src_a;
int src_r = (int)(clamped_r * 255.0f);
int src_g = (int)(clamped_g * 255.0f);
int src_b = (int)(clamped_b * 255.0f);
```

Then in the loop it reads the destination pixel:

```c
uint32_t dest_pixel = *dst;
int dest_r = (dest_pixel      ) & 0xFF;
int dest_g = (dest_pixel >>  8) & 0xFF;
int dest_b = (dest_pixel >> 16) & 0xFF;
```

Then blends per channel:

```c
int out_r = (src_r * src_a + dest_r * inv_alpha) >> 8;
int out_g = (src_g * src_a + dest_g * inv_alpha) >> 8;
int out_b = (src_b * src_a + dest_b * inv_alpha) >> 8;
```

Then packs the result back into one pixel.

The entire reason this path exists is simple:

```text
partial transparency depends on what was already there
```

## Table: The Three Alpha Modes

| Source alpha | What the rasterizer does | Why                                                 |
| ------------ | ------------------------ | --------------------------------------------------- |
| `a <= 0`     | return immediately       | fully transparent source changes nothing            |
| `a >= 1`     | direct write             | opaque source replaces destination                  |
| `0 < a < 1`  | read, blend, repack      | partially transparent source mixes with destination |

## Worked Example: Blend One Red Pixel Over Dark Gray

Suppose:

```text
source color = (255, 0, 0)
source alpha = 128
destination  = (40, 40, 40)
```

Then:

```text
inv_alpha = 255 - 128 = 127
```

### Red channel

```text
out_r = (255 * 128 + 40 * 127) >> 8
      = (32640 + 5080) >> 8
      = 37720 >> 8
      = 147
```

### Green channel

```text
out_g = (0 * 128 + 40 * 127) >> 8
      = 5080 >> 8
      = 19
```

### Blue channel

```text
out_b = 19
```

So the output pixel becomes approximately:

```text
(147, 19, 19)
```

That is exactly what a half-transparent red over dark gray should feel like.

## Step 8: Why the Code Uses `>> 8`

The exact blend formula would divide by `255`.

This implementation uses:

```text
>> 8
```

which is roughly division by `256`.

That is a deliberate small integer approximation:

- simpler arithmetic
- fast enough for the software renderer
- visually acceptable for this educational renderer

The lesson to keep is not “bit shifts are magic.”

The lesson is:

```text
the software renderer is doing explicit per-channel math
```

## Step 9: `draw_rect(...)` Is the Primitive That Later Text Rendering Reuses

This is the most important bridge into Lessons 10 and 11.

Later text rendering does not invent a new pixel-writing system.

Instead, glyph rendering turns font bits into many tiny rectangle writes.

So the rendering ladder is:

```text
draw_rect -> draw_char -> draw_text
```

If `draw_rect(...)` makes sense, the rest of the module becomes a composition story instead of a mystery.

## Practical Exercises

### Exercise 1: Predict the Clipped Bounds

Use this input:

```text
x = 7.5
y = -2.0
w = 5.0
h = 4.0
backbuffer width = 10
backbuffer height = 8
```

Compute `x0`, `y0`, `x1`, and `y1` by hand.

Expected result:

```text
x0 = 7
y0 = 0
x1 = 10
y1 = 2
```

### Exercise 2: Predict the Alpha Path

For each source alpha below, say which path the code takes:

```text
a = 0.0
a = 1.0
a = 0.35
```

Expected result:

```text
0.0  -> skip
1.0  -> opaque fast path
0.35 -> blend path
```

### Exercise 3: Predict the Visual Bug

Imagine someone replaced:

```c
int stride = backbuffer->pitch / backbuffer->bytes_per_pixel;
```

with:

```c
int stride = backbuffer->width;
```

Predict the risk.

Expected answer:

```text
the code would only remain correct while pitch happened to equal width * bytes_per_pixel
```

That is precisely the kind of contract shortcut the course is training you to avoid.

## Common Mistakes

### Mistake 1: Thinking clipping happens in the backend

It does not.

`draw_rect(...)` clips at the CPU rasterization stage before any upload happens.

### Mistake 2: Forgetting that partial alpha requires destination reads

If you do not read the old destination pixel, you cannot blend correctly.

### Mistake 3: Treating `draw_rect(...)` as a special-case helper instead of the renderer's base primitive

Later text rendering is built directly on top of this function.

## Troubleshooting Your Understanding

### “I understand clipping in theory but not in code”

Reduce it to one sentence:

```text
the function computes the visible rectangle before writing any pixels
```

### “I keep forgetting why there are two loop paths”

Because opaque drawing can overwrite directly, while partial alpha must preserve part of the old destination color.

## Recap

You now have the first full raster primitive in the course:

1. float inputs become integer clip bounds
2. `pitch` becomes a row stride
3. fully transparent rectangles do nothing
4. fully opaque rectangles use a fast write-only path
5. partially transparent rectangles read, blend, and repack the destination pixel

## Next Lesson

Now that the first backbuffer-writing primitive is solid, the next step is to build text on top of it.

Lesson 10 turns font table bits into scaled rectangle writes through `draw_char(...)`.
