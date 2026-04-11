# Lesson 08: Upload CPU Pixels to the Screen

## Frontmatter

- Module: `03-pixel-pipeline-and-drawing`
- Lesson: `08`
- Status: Required
- Target files:
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
  - `src/utils/backbuffer.h`
- Target symbols:
  - `display_backbuffer`
  - `UpdateTexture`
  - `DrawTexturePro`
  - `glTexImage2D`
  - `glTexSubImage2D`
  - `glXSwapBuffers`
  - `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`
  - `GL_RGBA`

## Observable Outcome

By the end of this lesson, you should be able to trace one frame of CPU-rendered pixels all the way to the actual window in both backends.

You should be able to explain the frame path from memory:

```text
game code writes backbuffer pixels
  -> backend uploads those pixels into a GPU texture
  -> backend draws that texture into the window
  -> present or swap makes the new frame visible
```

You should also be able to explain why both backends can do this without any red-blue channel swap.

## Prerequisite Bridge

Lesson 07 locked down the backbuffer contract.

You now know what one pixel looks like in memory.

The next necessary question is:

```text
how do those bytes become something the user can actually see?
```

This lesson answers that by walking the exact presentation bridge in both Raylib and X11.

## Why This Lesson Exists

Many beginners accidentally imagine software rendering like this:

```text
draw_rect somehow draws directly to the monitor
```

That is not the model here.

The real model is:

```text
CPU builds an image in memory
backend uploads that image to a GPU texture
backend draws one textured surface into the window
window presentation makes the frame visible
```

Once that becomes clear, backend presentation code stops looking like random API ceremony.

It becomes a bridge from CPU image memory to visible output.

## Shared Pipeline First

Before splitting into backend-specific APIs, compress both backends into one shared diagram:

```text
Backbuffer.pixels on CPU
        |
        v
upload latest bytes into GPU texture
        |
        v
draw that texture into the window
        |
        v
present the completed frame
```

The API names differ.
The model does not.

## Step 1: Raylib Creates a Texture That Matches the Backbuffer Layout

At startup or resize time, Raylib creates a texture from an `Image` descriptor:

```c
Image img = {
    .data = props->backbuffer.pixels,
    .width = new_w,
    .height = new_h,
    .mipmaps = 1,
    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
};
g_raylib.texture = LoadTextureFromImage(img);
SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);
```

### What each important line means

`.data = props->backbuffer.pixels`

- the shared CPU pixel memory is the source image

`.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`

- Raylib is told to interpret the bytes as `[R][G][B][A]`
- that matches the in-memory byte order from Lesson 07

`SetTextureFilter(..., TEXTURE_FILTER_POINT)`

- preserve crisp pixel edges when the image is scaled

## Step 2: Raylib's Per-Frame Upload Is Explicit

The Raylib presentation function begins with:

```c
UpdateTexture(g_raylib.texture, backbuffer->pixels);
```

That one line is the key bridge.

It means:

```text
copy the latest CPU-rendered pixel bytes into the GPU texture
```

After that, Raylib decides where the texture should appear in the window:

```c
Rectangle src = {0.0f, 0.0f, (float)backbuffer->width,
                 (float)backbuffer->height};
Rectangle dest = {off_x, off_y, dst_w, dst_h};

BeginDrawing();
ClearBackground(BLACK);
DrawTexturePro(g_raylib.texture, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
EndDrawing();
```

### Read that as a stage sequence

```text
1. upload latest CPU image into texture memory
2. decide where the texture should appear on screen
3. clear the window
4. draw one textured rectangle
5. let Raylib present the frame at EndDrawing()
```

## Visual: Raylib Presentation Flow

```text
backbuffer->pixels
      |
      v
UpdateTexture(texture, pixels)
      |
      v
texture now contains this frame's image
      |
      v
DrawTexturePro(texture, src, dest, ...)
      |
      v
window shows the textured image
```

## Step 3: X11 Allocates Texture Storage First

At startup or on resize, X11 allocates GPU texture storage with:

```c
glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_w, new_h, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
```

That call means:

- `GL_RGBA8` asks the GPU to reserve RGBA8 texture storage
- `GL_RGBA` says future source bytes will arrive as RGBA byte tuples
- `NULL` means allocate storage now without uploading actual pixels yet

This is the first important contrast with Raylib.

Raylib hides some of the texture management inside `LoadTextureFromImage(...)`.
X11 shows you the storage allocation more directly.

## Step 4: X11's Per-Frame Upload Uses `glTexSubImage2D`

Inside `display_backbuffer(...)`, the X11 backend uploads the latest CPU image like this:

```c
glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                backbuffer->width, backbuffer->height,
                GL_RGBA, GL_UNSIGNED_BYTE,
                backbuffer->pixels);
```

That is the X11 equivalent of Raylib's `UpdateTexture(...)`.

It means:

```text
reuse the existing GPU texture allocation
replace its pixel contents with the latest CPU image
```

## Step 5: X11 Draws One Textured Quad and Swaps Buffers

After upload, X11 draws one textured quad:

```c
glBegin(GL_QUADS);
glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
glEnd();

glXSwapBuffers(g_x11.display, g_x11.window);
```

Read this as:

```text
texture coordinates 0..1 cover the whole uploaded image
vertex coordinates decide where that image appears in the window
glXSwapBuffers presents the finished frame
```

So even though the API surface looks very different, the high-level job is the same as Raylib.

## Table: Raylib and X11 Side by Side

| Stage                    | Raylib                      | X11 / GLX                              |
| ------------------------ | --------------------------- | -------------------------------------- |
| allocate texture storage | `LoadTextureFromImage(...)` | `glTexImage2D(..., NULL)`              |
| upload current frame     | `UpdateTexture(...)`        | `glTexSubImage2D(...)`                 |
| draw texture into window | `DrawTexturePro(...)`       | textured quad with `glBegin(GL_QUADS)` |
| present frame            | `EndDrawing()`              | `glXSwapBuffers(...)`                  |

That comparison is worth memorizing because it keeps the backend differences in the correct box.

## Step 6: Why Neither Backend Needs a Channel Swap

Lesson 07 established the key memory fact:

```text
packed integer value: 0xAABBGGRR
in-memory bytes on little-endian hardware: [R][G][B][A]
```

Now compare that with the backend expectations.

Raylib expects:

```text
PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
```

OpenGL expects:

```text
GL_RGBA + GL_UNSIGNED_BYTE
```

Both describe RGBA byte order.

So both backends can read the same backbuffer bytes directly.

## Visual: Format Agreement Across the Whole Path

```text
CPU packed integer:   0xAABBGGRR
CPU memory bytes:     [R][G][B][A]

Raylib expects:       [R][G][B][A]
OpenGL expects:       [R][G][B][A]

result:
  same memory works in both backends
  no red-blue swap required
```

If either backend used BGRA instead, red and blue would appear swapped on screen.

## Step 7: Separate Texture Allocation from Per-Frame Upload

Students often blur these two together.

They are different jobs.

### Allocation

```text
make texture storage that is the right size and format
```

### Upload

```text
copy this frame's pixel bytes into that storage
```

In X11 those steps are clearly split between `glTexImage2D(...)` and `glTexSubImage2D(...)`.

In Raylib the split is more hidden, but the same separation still exists conceptually.

## Worked Example: Where the Software Renderer Actually Ends

Suppose the game runtime calls:

- `draw_rect(...)` twenty times
- `draw_text(...)` six times

At that point, nothing has been drawn to the window yet.

The software renderer has only changed CPU memory.

The visible window changes later, when the backend:

1. uploads `backbuffer->pixels`
2. draws the texture into the window
3. presents the frame

That is the most important boundary in this lesson:

```text
the software renderer builds the image
the backend presentation layer displays the image
```

## Step 8: Resize Changes Texture Management, Not the Core Model

When the backbuffer size changes, both backends must keep the GPU texture shape compatible with it.

### Raylib path

- resize backbuffer memory
- recreate the `Texture2D`
- keep using `UpdateTexture(...)` every frame

### X11 path

- resize backbuffer memory
- reissue `glTexImage2D(...)` with the new dimensions
- keep using `glTexSubImage2D(...)` every frame

### Shared lesson

```text
CPU image shape changes
backend texture shape must match
presentation model stays exactly the same
```

## Practical Exercises

### Exercise 1: Trace the Frame Path From Memory

Without looking back up, write the four-stage pipeline from memory:

```text
CPU image -> GPU upload -> textured draw -> present
```

Then reopen the lesson and check whether you mixed up upload and presentation.

### Exercise 2: Predict the Wrong-Format Failure

Imagine the X11 upload call changed from `GL_RGBA` to `GL_BGRA` while the CPU memory stayed unchanged.

Predict the symptom before trying to visualize the whole frame.

Expected answer:

```text
red and blue channels would appear swapped
```

### Exercise 3: Predict the Wrong-Size Failure

Suppose the backbuffer becomes `1024 x 768` but the backend keeps an `800 x 600` texture allocation.

Expected class of bug:

```text
partial uploads, distortion, or stale texture memory depending on the backend path
```

The exact visual artifact can vary, but the root cause is always a shape mismatch between the CPU image and GPU texture storage.

## Common Mistakes

### Mistake 1: Thinking `UpdateTexture(...)` or `glTexSubImage2D(...)` create the image

They do not.

They only upload the image that the CPU already built.

### Mistake 2: Thinking Raylib and X11 represent different rendering architectures

They use different APIs.
They do not use different presentation models.

### Mistake 3: Forgetting that presentation happens after upload

Upload changes texture contents.
Presentation makes the new frame visible.

## Troubleshooting Your Understanding

### “I understand the APIs but still cannot picture the frame path”

Reduce everything to this picture:

```text
memory image
  -> texture image
  -> window image
```

### “I keep thinking the backend is the renderer”

In this module, the backend is not doing rectangle rasterization or glyph decoding.

The backend is only presenting the finished CPU image.

## Recap

This lesson established the presentation bridge clearly:

1. the game runtime writes pixels into CPU memory
2. the backend uploads those bytes into a GPU texture
3. the backend draws that texture into the window
4. the window presents the frame

That is the full path from software-rendered bytes to a visible image.

## Next Lesson

Now that you know how the final image reaches the screen, the next question is:

```text
what is the first real primitive that writes the image into the backbuffer?
```

Lesson 09 answers that through `draw_rect(...)`, clipping, and alpha blending.
