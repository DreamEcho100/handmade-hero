# Module 03: Pixel Pipeline and Drawing

Module 03 is where the course stops talking mainly about boot ownership and starts talking about the actual image the runtime produces every frame.

Module 02 taught you that both backend shells present one shared CPU backbuffer.

Module 03 answers the next necessary question:

```text
what exactly is inside that backbuffer,
how do those bytes become visible on screen,
and how do simple drawing helpers build a readable HUD from it?
```

This is the module where the software renderer becomes concrete.

---

## Why This Module Exists

Many beginners conceptually blur together four different things:

- CPU pixel memory
- packed integer color values
- GPU texture upload
- drawing helpers like `draw_rect` and `draw_text`

When those layers blur together, software rendering feels magical.

This module removes that magic by teaching the pipeline in the correct order:

```text
memory contract
  -> upload bridge
  -> primitive rasterization
  -> glyph decoding
  -> string composition
```

That order matters. Later rendering modules depend on this model being stable.

---

## Observable Outcome

By the end of Module 03, you should be able to:

1. Explain the `Backbuffer` memory contract precisely.
2. Describe how one RGBA pixel is packed and stored in memory on this branch.
3. Trace CPU pixels through backend upload into a visible frame.
4. Explain how clipping, alpha blending, glyph decoding, and text composition all build on the same backbuffer rules.

If those ideas feel automatic, later HUD and world-rendering lessons will land much more easily.

---

## Module Map

```text
Lesson 07
  backbuffer memory layout and color packing

Lesson 08
  upload CPU pixels to the screen in Raylib and X11

Lesson 09
  draw_rect, clipping, and alpha blending

Lesson 10
  bitmap font table and draw_char

Lesson 11
  draw_text, cursor movement, and runtime labels
```

---

## Lesson Order

1. [lesson-07-backbuffer-memory-layout-and-color-packing.md](lesson-07-backbuffer-memory-layout-and-color-packing.md)
   - learn what one pixel buffer actually is
2. [lesson-08-upload-cpu-pixels-to-the-screen.md](lesson-08-upload-cpu-pixels-to-the-screen.md)
   - trace the CPU image into both presentation backends
3. [lesson-09-draw-rect-clipping-and-alpha-blending.md](lesson-09-draw-rect-clipping-and-alpha-blending.md)
   - learn the first real raster primitive
4. [lesson-10-bitmap-font-table-and-draw-char.md](lesson-10-bitmap-font-table-and-draw-char.md)
   - decode glyph bits into visible pixels
5. [lesson-11-draw-text-and-debug-labels.md](lesson-11-draw-text-and-debug-labels.md)
   - assemble glyphs into strings and dynamic runtime labels

---

## How To Study This Module

Do not read this module passively.

For each lesson:

1. Draw the memory or pipeline diagram by hand.
2. Compute at least one real pixel address or one real packed color value yourself.
3. Connect the helper you are reading to the exact backbuffer fields it depends on.
4. Say what changes at the CPU layer and what changes only at the backend presentation layer.

The tiny calculations matter. They turn the lessons from vocabulary into actual operational understanding.

---

## Core Habits To Build Here

Module 03 is training four habits that stay useful for the entire renderer path.

### Habit 1: Separate memory layout from presentation API

The backbuffer contract exists before Raylib or X11 touches it.

### Habit 2: Treat `pitch` as a stride, not just an extra number

If you forget that `pitch` is measured in bytes, row walking becomes unsafe very quickly.

### Habit 3: Treat drawing helpers as consequences of the memory model

`draw_rect`, `draw_char`, and `draw_text` are not unrelated tricks. They are different ways of writing the same backbuffer.

### Habit 4: Treat backend upload as a bridge, not as the renderer itself

The CPU renderer builds the image.
The backend upload step presents it.

---

## Beginner Danger Zones

Watch for these mistakes while reading Module 03:

1. Thinking `0xAARRGGBB` means the in-memory bytes are stored in that same left-to-right order.
2. Forgetting that little-endian storage changes how those bytes appear in memory.
3. Treating `UpdateTexture(...)` or `glTexSubImage2D(...)` as if they create the image instead of merely uploading it.
4. Thinking text rendering is a special subsystem unrelated to rectangle drawing.

This module exists largely to prevent those mistakes.

---

## Verification Before Module 04

Before moving on, you should be able to explain all of these without reopening the files:

1. What one pixel looks like in memory on this branch.
2. What `pitch` means and why row walking depends on it.
3. How the CPU backbuffer reaches the screen in both backends.
4. Why clipping and alpha blending are backbuffer rules rather than backend rules.
5. Why `draw_text(...)` is really just repeated glyph decoding and rectangle writes.

If those answers feel stable, you are ready for the next major topic: input snapshots, timing, and presentation policy.

---

## Bridge To Module 04

Module 03 gives you a trustworthy image pipeline.

Module 04 asks the next systems question:

```text
how does the runtime decide what to draw each frame,
when to draw it,
and how input and presentation policy affect that loop?
```

That means the next module will shift from pixel memory into frame-level behavior.
