# Lesson 10: Bitmap Font Table and `draw_char`

## Frontmatter

- Module: `03-pixel-pipeline-and-drawing`
- Lesson: `10`
- Status: Required
- Target files:
  - `src/utils/draw-text.c`
  - `src/utils/draw-text.h`
  - `src/utils/draw-shapes.c`
- Target symbols:
  - `FONT_8X8`
  - `draw_char`
  - `draw_rect`

## Observable Outcome

By the end of this lesson, you should be able to explain how one ASCII character becomes visible pixels in the backbuffer.

You should be able to explain the full chain:

```text
character code
  -> select glyph rows from FONT_8X8
  -> test each bit in each row
  -> for every set bit, draw a scaled pixel block
  -> resulting blocks form the glyph image
```

## Prerequisite Bridge

Lesson 09 gave you the base drawing primitive: `draw_rect(...)`.

This lesson answers the next composition question:

```text
how do we build readable symbols out of repeated rectangle writes?
```

The answer is: a bitmap font table plus one glyph raster helper.

## Why This Lesson Exists

Text rendering often looks mysterious because people encounter it first through large font engines.

That is not the right first model for this course.

Here the job is intentionally small and explicit:

1. store tiny glyphs as bits
2. read those bits row by row
3. convert each `1` bit into a visible block of pixels

That makes text rendering feel like a direct extension of the rectangle rasterizer you already understand.

## Step 1: Understand What `FONT_8X8` Stores

In `src/utils/draw-text.c`, the font is stored as a table of bytes indexed by character code.

Conceptually, the structure is:

```text
FONT_8X8[character][row]
```

That means:

- each character has 8 rows
- each row is one byte
- each bit in that byte says whether one pixel of the glyph is on or off

So one glyph is an `8 x 8` bitmap.

## Visual: One Glyph as Eight Bit Rows

Imagine a glyph stored like this:

```text
row 0: 00111100
row 1: 01000010
row 2: 10000001
row 3: 11111111
row 4: 10000001
row 5: 10000001
row 6: 10000001
row 7: 00000000
```

Each `1` means:

```text
draw this pixel block
```

Each `0` means:

```text
leave this pixel block empty
```

That is all a bitmap font is at this scale.

## Step 2: Start With the Live `draw_char(...)` Interface

The renderer exposes a helper that conceptually does this job:

```text
draw one character at (x, y)
using a chosen scale and color
```

The important architectural point is not the exact argument order.

It is this:

```text
draw_char does not write pixels directly with custom low-level loops
it reuses draw_rect for every lit font bit
```

That means `draw_char(...)` is a composition layer, not a second renderer.

## Step 3: Select the Glyph Rows From the Character Code

At a high level, the function does something like:

```c
unsigned char glyph_index = (unsigned char)c;
const uint8_t *glyph = FONT_8X8[glyph_index];
```

This is the first key idea:

```text
the incoming character is just an index into the font table
```

So the renderer does not “understand letters” in a language sense.

It only understands:

```text
character code -> 8 rows of bits
```

## Step 4: Walk the Glyph Row by Row

The natural loop structure is:

```c
for (int row = 0; row < 8; row++) {
  uint8_t bits = glyph[row];
  for (int col = 0; col < 8; col++) {
    ... test one bit ...
  }
}
```

That means the renderer asks:

```text
for each row,
which columns should be visible?
```

This is just rasterization over a tiny logical grid.

## Step 5: Read Each Bit as an On/Off Pixel Decision

Within one row, the code tests whether a given glyph pixel is set.

Conceptually:

```c
if (bits & mask_for_this_column) {
  ... draw one scaled block ...
}
```

That is the heart of the whole lesson.

Each font bit becomes a yes/no answer to the question:

```text
should this glyph cell be painted?
```

## Worked Example: Decode One Row

Suppose one row byte is:

```text
00101100
```

If the renderer treats the row from left to right, the visible columns are the positions containing `1` bits.

That yields:

```text
col 2 -> on
col 4 -> on
col 5 -> on
```

All other columns remain empty.

So that one row contributes three painted blocks to the final glyph.

## Step 6: Convert Glyph Grid Coordinates Into Backbuffer Coordinates

When a bit is set at `(col, row)`, the actual screen-space position is based on:

```text
pixel_x = start_x + col * scale
pixel_y = start_y + row * scale
```

If `scale == 1`, each font bit becomes a `1 x 1` pixel write.

If `scale == 2`, each font bit becomes a `2 x 2` block.

If `scale == 4`, each font bit becomes a `4 x 4` block.

This is why bitmap fonts scale so naturally in a pixel-art renderer.

## Step 7: `draw_char(...)` Reuses `draw_rect(...)`

Instead of writing raw pixels directly, the function does something conceptually like:

```c
draw_rect(backbuffer,
          pixel_x,
          pixel_y,
          scale,
          scale,
          r, g, b, a);
```

This is a very good design choice for the course.

It means:

```text
glyph rendering inherits clipping
glyph rendering inherits alpha handling
glyph rendering inherits the same packed color rules
```

So Lesson 10 is not introducing a competing pixel pipeline.

It is building on the one you already understand.

## Visual: One Character as Tiny Rectangles

```text
glyph bit grid (8x8)
    1 bit on
       |
       v
draw_rect at character-space cell
       |
       v
scaled block lands in the backbuffer
```

Repeat that for every lit bit and the whole glyph appears.

## Step 8: Why an 8x8 Font Is Enough for Debug UI

This course is not trying to build a full typography stack.

The runtime only needs:

- labels
- debug values
- overlays
- simple world annotations

An `8 x 8` bitmap font is enough because it is:

- tiny
- deterministic
- easy to scale
- easy to teach from first principles

That keeps the lesson focused on renderer architecture, not font engine complexity.

## Worked Example: Render a 3x Scale Letter

Suppose the character `'A'` contains 18 set bits in its `8 x 8` glyph.

If `scale = 3`, then each set bit becomes one `3 x 3` filled block.

So the final rendered letter is made from:

```text
18 rectangle calls
each covering 9 actual backbuffer pixels
```

That means one small bitmap glyph becomes a much larger readable character without changing the underlying font data.

## Step 9: The Most Important Mental Compression

Compress the entire lesson into one sentence:

```text
draw_char reads glyph bits and turns each lit bit into a small rectangle in the backbuffer
```

If that sentence feels concrete, then the implementation is no longer mysterious.

## Practical Exercises

### Exercise 1: Count the Draw Calls

Suppose a glyph row pattern contains 3 set bits in row 0, 2 in row 1, 4 in row 2, and 0 in the remaining rows.

How many `draw_rect(...)` calls does `draw_char(...)` make for that glyph?

Expected result:

```text
3 + 2 + 4 = 9 calls
```

### Exercise 2: Predict the Screen Position

Assume:

```text
start_x = 100
start_y = 40
scale   = 2
lit bit at (col = 5, row = 3)
```

Compute the rectangle origin.

Expected result:

```text
x = 100 + 5 * 2 = 110
y = 40  + 3 * 2 = 46
```

### Exercise 3: Predict the Consequence of Reusing `draw_rect(...)`

If a glyph is partially off-screen, what part of the system keeps it safe?

Expected answer:

```text
draw_rect performs clipping, so draw_char inherits safe off-screen handling automatically
```

## Common Mistakes

### Mistake 1: Thinking the font table stores colors

It does not.

The font table only stores shape information as bits.

### Mistake 2: Thinking `draw_char(...)` needs a totally separate rasterizer

It does not.

It composes the existing rectangle primitive.

### Mistake 3: Forgetting that scaling affects block size, not the font table itself

The glyph data stays `8 x 8`.

Scale only changes how large each lit bit is when drawn.

## Troubleshooting Your Understanding

### “I can read the font bytes, but I still do not picture the glyph”

Sketch one row as eight boxes and mark the `1` bits.

Then repeat for eight rows.

That manual exercise usually makes bitmap fonts click immediately.

### “Why does text rendering feel simpler here than in normal UI systems?”

Because this is intentionally a tiny debug-font renderer, not a general-purpose text layout engine.

## Recap

You now know how one character is rendered:

1. use the character code to index the font table
2. read 8 rows of 8 bits
3. test each bit
4. for every lit bit, draw a scaled rectangle
5. the set of rectangles forms the glyph

## Next Lesson

The next step is the obvious composition layer:

```text
how do multiple characters become labels, debug values, and runtime text strings?
```

Lesson 11 covers `draw_text(...)` and real debug-label usage in the game runtime.
