# Lesson 07: Backbuffer Memory Layout, Packed Color, and Resize Invariants

## Frontmatter

- Module: `03-pixel-pipeline-and-drawing`
- Lesson: `07`
- Status: Required
- Target files:
  - `src/utils/backbuffer.h`
  - `src/utils/backbuffer.c`
  - `src/platform.h`
- Target symbols:
  - `Backbuffer`
  - `GAME_RGBA`
  - `GAME_RGB`
  - `backbuffer_resize`
  - `platform_game_props_init`

## Observable Outcome

By the end of this lesson, you should be able to treat the backbuffer as a precise memory contract instead of a vague “thing the renderer writes to.”

You should be able to:

- explain what every `Backbuffer` field means
- compute the location of one pixel from `(x, y)` using `pitch`
- explain why `GAME_RGBA(r, g, b, a)` produces an integer that reads as `0xAABBGGRR` in hex but is still stored as `[R][G][B][A]` bytes in memory on little-endian hardware
- explain what must be updated when the backbuffer is resized
- explain why both backends can trust the same pixel bytes later

## Prerequisite Bridge

Module 02 established a very important architecture rule:

```text
both backends share one CPU-owned image buffer
```

That is a good high-level statement, but it is not operational enough yet.

This lesson narrows the question from:

```text
who owns the image?
```

to:

```text
what exactly is the image in memory?
```

That is the question you must answer before rectangles, glyphs, alpha blending, or texture upload can make sense.

## Why This Lesson Exists

Most beginner confusion in software rendering starts before any drawing math.

The common failure mode looks like this:

```text
I know there is a pixel buffer,
but I do not know how rows are laid out,
how colors are packed,
or why the upload code trusts the bytes.
```

If that stays fuzzy, every later rendering helper feels like a special trick.

This lesson removes that fuzziness by locking down five concrete ideas:

1. the backbuffer is plain CPU memory
2. rows are walked using byte stride, not width alone
3. colors are packed into one `uint32_t`
4. little-endian storage changes how that packed integer sits in memory
5. resize must preserve the same contract, not invent a new one

## Step 1: Read `Backbuffer` as Plain Data

The live type in `src/utils/backbuffer.h` is:

```c
typedef struct {
  uint32_t *pixels;
  int       width;
  int       height;
  int       pitch;
  int       bytes_per_pixel;
} Backbuffer;
```

This type is deliberately boring.

That is good.

There is no GPU texture handle here.
There is no Raylib-specific field.
There is no X11-specific field.

It is just:

```text
pointer + dimensions + stride + pixel format size
```

### What each field means

`pixels`

- the base address of the CPU-owned image
- every draw helper eventually writes through this pointer

`width`

- logical number of pixels in one row

`height`

- logical number of rows in the image

`pitch`

- bytes per row
- row stepping uses this field, not `width` directly

`bytes_per_pixel`

- size of one stored pixel
- `4` on this branch because pixels are RGBA8888

## Visual Model: The Backbuffer as Rows

```text
pixels
  |
  +--> row 0: [p00][p01][p02][p03] ...
  +--> row 1: [p10][p11][p12][p13] ...
  +--> row 2: [p20][p21][p22][p23] ...
  |
  +--> row y: [py0][py1][py2][py3] ...

width  = how many pixels exist in one row
height = how many rows exist total
pitch  = how many bytes to jump to reach the next row
```

The most important distinction in this lesson is:

```text
width counts pixels
pitch counts bytes
```

Do not collapse those into one idea.

## Step 2: Learn the Pixel Address Formula Once

If you want pixel `(x, y)`, the byte-level formula is:

```text
row_start  = base_pointer + y * pitch
pixel_addr = row_start + x * bytes_per_pixel
```

Because `pixels` is a `uint32_t *`, most draw code converts that to a pixel stride first:

```text
stride_in_pixels = pitch / bytes_per_pixel
index            = y * stride_in_pixels + x
```

That is why later code computes:

```c
int stride = backbuffer->pitch / backbuffer->bytes_per_pixel;
```

instead of assuming that `stride == width` forever.

## Worked Example: Compute One Pixel Address

Assume:

```text
width           = 6
height          = 4
bytes_per_pixel = 4
pitch           = 24
target pixel    = (x = 4, y = 2)
```

### Step 1: Compute pixel stride

```text
stride = pitch / bytes_per_pixel
       = 24 / 4
       = 6
```

### Step 2: Compute the index into `uint32_t *pixels`

```text
index = y * stride + x
      = 2 * 6 + 4
      = 16
```

So the pixel lives at:

```text
pixels[16]
```

### Step 3: Compute byte offset from the start of the image

```text
offset = y * pitch + x * bytes_per_pixel
       = 2 * 24 + 4 * 4
       = 48 + 16
       = 64 bytes
```

That is the entire row-walking model in one calculation.

## Step 3: Read `GAME_RGBA` as Bit Packing

The live macro is:

```c
#define GAME_RGBA(r, g, b, a) \
  ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
    ((uint32_t)(g) <<  8) |  (uint32_t)(r) )
```

At the integer level, this packs the channels into one 32-bit value like this:

```text
bits 24..31 -> A
bits 16..23 -> B
bits  8..15 -> G
bits  0.. 7 -> R
```

So if you only look at the integer value, it reads like:

```text
0xAABBGGRR
```

That is the exact result produced by the macro as written.

This is the place where students often get confused, because the file comments and upload rules talk about in-memory bytes as `[R][G][B][A]`.

Both statements can be true at once.

## Step 4: Separate Integer Layout from Memory Layout

The macro defines the integer bit layout.

Memory layout depends on endianness.

On the little-endian CPUs the course assumes, the least-significant byte is stored at the lowest address.

That means the integer:

```text
0xAABBGGRR
```

appears in memory as:

```text
[RR][GG][BB][AA]
```

So the two correct statements are:

```text
packed integer value: 0xAABBGGRR
stored byte order:    [R][G][B][A]
```

That is exactly why the backend upload code can use RGBA formats with no swap.

## Worked Example: One Real Packed Pixel

Take this call:

```text
GAME_RGBA(0x11, 0x22, 0x33, 0x44)
```

### Step 1: Shift each channel into place

```text
A -> 0x44000000
B -> 0x00330000
G -> 0x00002200
R -> 0x00000011
```

### Step 2: OR them together

```text
result = 0x44332211
```

### Step 3: Ask what bytes appear in memory on little-endian hardware

```text
lowest address  -> 0x11 -> R
next            -> 0x22 -> G
next            -> 0x33 -> B
highest address -> 0x44 -> A
```

So the integer hex string and the in-memory byte order are describing different views of the same value.

That distinction matters for the rest of the module.

## Step 5: `GAME_RGB` Is Just the Opaque Shortcut

The companion helper is:

```c
#define GAME_RGB(r, g, b) GAME_RGBA((r), (g), (b), 255)
```

This does not introduce a new format.

It just says:

```text
use the same packing rule
force alpha to fully opaque
```

That keeps opaque color construction readable without changing the underlying pixel contract.

## Step 6: Boot Initialization Locks the Contract In

Inside `platform_game_props_init(...)`, the backbuffer is initialized like this:

```c
props->backbuffer.width = GAME_W;
props->backbuffer.height = GAME_H;
props->backbuffer.bytes_per_pixel = 4;
props->backbuffer.pitch = GAME_W * 4;
props->backbuffer.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
memset(props->backbuffer.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);
```

That code establishes the startup invariants that later draw and upload code depend on:

```text
one pixel is 4 bytes
pitch is width * 4
pixels points at enough storage for width * height pixels
the initial image starts cleared to zero
```

## Visual: The Contract at Boot

```text
platform_game_props_init
  -> allocate width * height * 4 bytes
  -> store pointer in backbuffer.pixels
  -> record width and height
  -> record bytes_per_pixel = 4
  -> compute pitch = width * 4
```

If any one of those fields disagreed with the actual allocation, later rendering would be wrong.

## Step 7: `backbuffer_resize(...)` Must Preserve Metadata Correctness

The live resize helper is:

```c
int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height) {
  if (new_width == backbuffer->width && new_height == backbuffer->height) {
    return 0;
  }

  size_t new_size = (size_t)new_width * (size_t)new_height * 4;
  uint32_t *new_pixels = (uint32_t *)realloc(backbuffer->pixels, new_size);
  if (!new_pixels) {
    return -1;
  }

  backbuffer->pixels = new_pixels;
  backbuffer->width = new_width;
  backbuffer->height = new_height;
  backbuffer->bytes_per_pixel = 4;
  backbuffer->pitch = new_width * 4;

  return 0;
}
```

The crucial lesson here is that resize is not just a memory allocation event.

It is a metadata repair event too.

After resize succeeds, all fields that describe the storage must be updated so they still agree with the actual image shape.

## Table: What Changes and What Stays Stable on Resize

| Field or property   | After successful resize | Why                                    |
| ------------------- | ----------------------- | -------------------------------------- |
| `pixels`            | may change              | `realloc(...)` can move the block      |
| `width`             | changes                 | the logical canvas width changed       |
| `height`            | changes                 | the logical canvas height changed      |
| `pitch`             | changes                 | bytes per row must match the new width |
| `bytes_per_pixel`   | stays `4`               | the pixel format did not change        |
| packed channel rule | unchanged               | still the same RGBA8888 contract       |

## Worked Example: Why Invariants Matter

Suppose resize changed `width` from `800` to `1024` but forgot to update `pitch`.

Then row stepping would still behave as if each row were `800 * 4 = 3200` bytes wide.

But the new storage really needs:

```text
1024 * 4 = 4096 bytes per row
```

So every row walk after row 0 would land too early.

That means:

- later draw helpers would scribble across the wrong pixels
- upload code would faithfully present corrupted rows
- the bug would look like a rendering bug even though the root cause is metadata drift

That is exactly why this lesson treats the backbuffer as a contract, not just a pointer.

## Practical Exercises

### Exercise 1: Compute a Pixel Index by Hand

Use this configuration:

```text
width = 10
height = 6
bytes_per_pixel = 4
pitch = 40
target = (x = 7, y = 4)
```

Compute:

1. stride in pixels
2. `pixels[...]` index
3. byte offset

Expected result:

```text
stride = 10
index  = 47
offset = 188 bytes
```

### Exercise 2: Predict the Memory Bytes

Take:

```text
GAME_RGBA(0xAA, 0xBB, 0xCC, 0xDD)
```

Predict:

1. the packed integer value
2. the in-memory byte order on little-endian hardware

Expected result:

```text
integer = 0xDDCCBBAA
bytes   = [AA][BB][CC][DD]
```

### Exercise 3: Predict the Resize Failure Mode

Imagine a bug where `backbuffer->pitch` is left unchanged after a successful resize.

Before reading Lesson 08, say out loud what you expect to happen:

```text
rows will be stepped incorrectly,
so the backend will upload a corrupted image,
not a correctly reshaped image
```

## Common Mistakes

### Mistake 1: Treating integer packing and memory byte order as the same question

They are related, but they are not the same.

The macro defines the integer.
The CPU endianness decides how that integer sits in memory.

### Mistake 2: Treating `pitch` as a synonym for `width`

`width` counts pixels.
`pitch` counts bytes.

Later rasterizers need the byte-based interpretation to step rows correctly.

### Mistake 3: Treating resize as “just grow the array”

Resize only succeeds conceptually if both the storage and the metadata still agree.

## Troubleshooting Your Understanding

### “The comments say `[R][G][B][A]`, but the macro looks backwards”

That is because the macro describes the integer bit layout, not the literal byte sequence in RAM.

On little-endian machines:

```text
low bits are stored first in memory
```

So the apparently “backwards” integer still becomes `[R][G][B][A]` in memory.

### “Why does the course care so much about `pitch` if it currently equals `width * 4`?”

Because good renderer code follows the contract, not the current coincidence.

Using `pitch` now makes the code correct even if row layout changes later.

## Recap

You now have the byte-level model the rest of Module 03 depends on:

1. the backbuffer is plain CPU memory plus metadata
2. row stepping depends on `pitch`
3. pixels are packed into one `uint32_t`
4. the integer bit layout and the in-memory byte order are not the same thing
5. resize must preserve the same contract after changing dimensions

## Next Lesson

Now that the backbuffer contract is stable, the next question is:

```text
how do those CPU bytes become visible in the actual window?
```

Lesson 08 follows the exact same pixel bytes through Raylib and X11 presentation.

\*\*\* Add File: /home/viavi/Desktop/workspaces/github/DreamEcho100/handmade-hero/ai-llm-knowledge-dump/generated-courses/platform-backend/course/lessons/03-pixel-pipeline-and-drawing/lesson-08-upload-cpu-pixels-to-the-screen.md

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

\*\*\* Add File: /home/viavi/Desktop/workspaces/github/DreamEcho100/handmade-hero/ai-llm-knowledge-dump/generated-courses/platform-backend/course/lessons/03-pixel-pipeline-and-drawing/lesson-09-draw-rect-clipping-and-alpha-blending.md

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

Lesson 10 turns font table bits into scaled rectangle writes through `draw_char(...)`.# Lesson 07: Backbuffer Memory Layout, Packed Color, and Resize Invariants

## Frontmatter

- Module: `03-pixel-pipeline-and-drawing`
- Lesson: `07`
- Status: Required
- Target files:
  - `src/utils/backbuffer.h`
  - `src/utils/backbuffer.c`
  - `src/platform.h`
- Target symbols:
  - `Backbuffer`
  - `GAME_RGBA`
  - `GAME_RGB`
  - `backbuffer_resize`
  - `platform_game_props_init`

## Observable Outcome

By the end of this lesson, you should be able to explain the backbuffer as a plain memory contract instead of a vague graphics object.

You should be able to do all of these without guessing:

- describe what each `Backbuffer` field means
- compute the address of one pixel using `pitch`
- explain why `GAME_RGBA(r, g, b, a)` produces the integer `0xAARRGGBB`
- explain why the in-memory bytes are still `[R][G][B][A]` on this branch
- explain what must change, and what must stay consistent, when the backbuffer is resized

## Prerequisite Bridge

Lesson 06 ended with one important ownership claim:

```text
both backends share one backbuffer contract
```

This lesson turns that claim into a precise mental model.

The question narrows from:

```text
who owns the backbuffer?
```

to:

```text
what exactly is the backbuffer?
```

## Why This Lesson Exists

Most beginner confusion in software rendering starts before any drawing math.

The usual failure mode looks like this:

```text
I know there is a pixel buffer,
but I do not know how rows are laid out,
how colors are packed,
or why the upload code trusts those bytes.
```

If that remains fuzzy, then rectangles, glyphs, alpha blending, and GPU upload all feel like separate unrelated tricks.

They are not separate tricks.

They are all consequences of one memory layout.

## Step 1: Read `Backbuffer` as Plain Data

The live type is:

```c
typedef struct {
  uint32_t *pixels;
  int width;
  int height;
  int pitch;
  int bytes_per_pixel;
} Backbuffer;
```

This type is intentionally boring.

That is a good thing.

There is no GPU handle here.
There is no Raylib-specific field here.
There is no X11-specific field here.

It is just:

```text
pointer + shape + stride + pixel-size metadata
```

### What each field means

`pixels`

- raw CPU-owned pixel memory
- every draw helper ultimately writes through this pointer

`width` and `height`

- logical canvas dimensions in pixels

`pitch`

- bytes per row
- used to move from row `y` to row `y + 1`

`bytes_per_pixel`

- storage size of one pixel
- `4` on this branch because the format is RGBA8888

## Visual: Row Layout

```text
pixels
  -> row 0: [p00][p01][p02]...[p0w]
  -> row 1: [p10][p11][p12]...[p1w]
  -> row 2: [p20][p21][p22]...[p2w]

width  = pixels per row
pitch  = bytes per row
height = number of rows
```

Do not merge `width` and `pitch` into one idea.

One counts pixels.
The other counts bytes.

## Step 2: Learn the Pixel Address Formula Once

If you want pixel `(x, y)`, the conceptual address formula is:

```text
row_start  = base_pointer + y * pitch bytes
pixel_addr = row_start + x * bytes_per_pixel
```

If you convert that into an index on `uint32_t *pixels`, the equivalent is:

```text
stride_in_pixels = pitch / bytes_per_pixel
index            = y * stride_in_pixels + x
```

That is why later draw helpers compute a stride from `pitch` instead of assuming width alone is enough.

## Worked Example: Compute One Pixel Location

Suppose the default startup backbuffer is:

```text
width           = 800
height          = 600
bytes_per_pixel = 4
pitch           = 3200
```

Now find pixel `(x = 10, y = 3)`.

### Step 1: Compute stride in pixels

```text
stride = pitch / bytes_per_pixel
       = 3200 / 4
       = 800
```

### Step 2: Compute the index

```text
index = y * stride + x
      = 3 * 800 + 10
      = 2410
```

So the target pixel is:

```text
pixels[2410]
```

### Step 3: Compute byte offset

```text
offset = y * pitch + x * bytes_per_pixel
       = 3 * 3200 + 10 * 4
       = 9640 bytes
```

That is the whole row-walking model.

## Step 3: Read `GAME_RGBA` as Bit Packing

The core packing macro is:

```c
#define GAME_RGBA(r, g, b, a) \
  ( ((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
    ((uint32_t)(g) <<  8) |  (uint32_t)(r) )
```

At the integer level, this produces:

```text
0xAARRGGBB
```

More explicitly:

```text
bits  0- 7  -> R
bits  8-15  -> G
bits 16-23  -> B
bits 24-31  -> A
```

So the packed integer layout is:

```text
31                    24 23                    16 15          8 7          0
+----------------------+------------------------+--------------+------------+
|          A           |           B            |      G       |     R      |
+----------------------+------------------------+--------------+------------+
```

## Step 4: Why the In-Memory Bytes Are Still `[R][G][B][A]`

This is the place where many beginners get tripped up.

The integer value is `0xAARRGGBB`, but on a little-endian CPU the least-significant byte is stored at the lowest memory address.

Since `R` occupies bits `0-7`, its byte lands first in memory.

So the in-memory byte order on this branch is:

```text
[R][G][B][A]
```

That is exactly why both backends can upload the same pixel memory using RGBA formats without a channel swap.

## Worked Example: Pack One Real Pixel

Take this call:

```text
GAME_RGBA(0x11, 0x22, 0x33, 0x44)
```

### Step 1: Put each channel in its bit lane

```text
A -> 0x44000000
B -> 0x00330000
G -> 0x00002200
R -> 0x00000011
```

### Step 2: OR them together

```text
result = 0x44332211
```

### Step 3: Ask what bytes appear in memory on little-endian hardware

```text
lowest address  -> 0x11 -> R
next            -> 0x22 -> G
next            -> 0x33 -> B
highest address -> 0x44 -> A
```

So the integer and the in-memory bytes are telling compatible but different stories.

That distinction is extremely important.

## Step 5: `GAME_RGB` Is Just the Opaque Shortcut

The helper:

```c
#define GAME_RGB(r, g, b) GAME_RGBA((r), (g), (b), 255)
```

simply means:

```text
same packing rule
alpha forced to 255
```

That gives you an opaque convenience macro without changing the underlying pixel format.

## Step 6: Shared Startup Establishes the Initial Invariants

Inside `platform_game_props_init(...)`, the initial backbuffer setup is:

```c
props->backbuffer.width = GAME_W;
props->backbuffer.height = GAME_H;
props->backbuffer.bytes_per_pixel = 4;
props->backbuffer.pitch = GAME_W * 4;
props->backbuffer.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
```

This establishes the important startup invariants:

```text
bytes_per_pixel = 4
pitch = width * 4
pixels points at valid storage for width * height pixels
```

Those invariants are what later draw and upload code rely on.

## Step 7: `backbuffer_resize(...)` Must Preserve Contract Integrity

The resize helper is:

```c
int backbuffer_resize(Backbuffer *backbuffer, int new_width, int new_height) {
  if (new_width == backbuffer->width && new_height == backbuffer->height) {
    return 0;
  }

  size_t new_size = (size_t)new_width * (size_t)new_height * 4;
  uint32_t *new_pixels = (uint32_t *)realloc(backbuffer->pixels, new_size);
  if (!new_pixels) {
    return -1;
  }

  backbuffer->pixels = new_pixels;
  backbuffer->width = new_width;
  backbuffer->height = new_height;
  backbuffer->bytes_per_pixel = 4;
  backbuffer->pitch = new_width * 4;

  return 0;
}
```

The important lesson is not just `realloc(...)`.

The important lesson is that after a successful resize, every field that describes the storage must still agree with the new shape.

## Visual: What Changes and What Stays Stable on Resize

```text
changes:
  pixels pointer may change
  width changes
  height changes
  pitch changes

stays stable:
  bytes_per_pixel stays 4
  one pixel is still packed RGBA8888
  row-walking still uses pitch and bpp
```

That is the resize invariant story.

## Common Beginner Mistakes

### Mistake 1: Thinking `0xAARRGGBB` directly describes memory byte order

It describes the integer value layout, not the literal left-to-right memory bytes.

### Mistake 2: Forgetting that `pitch` is measured in bytes

If you treat it as pixels, your row stepping logic becomes wrong.

### Mistake 3: Treating resize as just storage growth

Resize also has to preserve metadata correctness.

## Practice

Answer these before moving on:

1. Why is `pitch` needed if `width` already exists?
2. Why can the integer be `0xAARRGGBB` while memory bytes are `[R][G][B][A]`?
3. Why does `backbuffer_resize(...)` update both the pointer and the metadata fields?
4. What invariant lets both backends trust the same pixel memory later?

## Mini Exercises

### Exercise 1

Assume `width = 4`, `bytes_per_pixel = 4`, `pitch = 16`.

What is the index of pixel `(x = 2, y = 1)`?

### Exercise 2

Compute the packed integer for:

```text
GAME_RGBA(0xAA, 0xBB, 0xCC, 0xDD)
```

Then write the in-memory byte order on little-endian hardware.

## Troubleshooting Your Understanding

### "I keep mixing up integer representation and byte layout"

Keep two separate sentences in your head:

```text
packed integer value = 0xAARRGGBB
memory bytes on little-endian = [R][G][B][A]
```

### "I understand `pixels`, but not why metadata matters so much"

Because the pointer alone does not tell later code:

- how wide a row is
- how many rows exist
- how many bytes to skip between rows
- how large each pixel is

The metadata is what makes raw memory usable.

## Recap

You now know that the backbuffer is a plain shared memory contract with four critical ideas:

1. pixels live in CPU memory
2. row walking depends on pitch and bytes per pixel
3. colors are packed as `0xAARRGGBB`
4. little-endian storage still yields `[R][G][B][A]` bytes in memory

That is the foundation every later drawing lesson depends on.

## Next Lesson

Now that you know what the CPU image looks like in memory, the next question becomes:

```text
how does that CPU image reach the actual window in both backends?
```

Lesson 08 answers that by tracing the upload and presentation bridge.
