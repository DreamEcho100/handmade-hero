# Lesson 17: Scale Modes, Resize Events, and Backbuffer Reallocation Policy

## Frontmatter

- Module: `04-input-timing-and-presentation`
- Lesson: `17`
- Status: Required
- Target files:
  - `src/utils/base.h`
  - `src/platform.h`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
  - `src/utils/backbuffer.c`
- Target symbols:
  - `WindowScaleMode`
  - `WINDOW_SCALE_MODE_FIXED`
  - `WINDOW_SCALE_MODE_DYNAMIC_MATCH`
  - `WINDOW_SCALE_MODE_DYNAMIC_ASPECT`
  - `apply_scale_mode`
  - `backbuffer_resize`
  - `ConfigureNotify`
  - `IsWindowResized`

## Observable Outcome

By the end of this lesson, you should be able to explain what happens after a resize event in each scale mode, including when the software backbuffer changes size and when only presentation placement changes.

You should also be able to explain why resize handling has two layers:

- policy: what size should the backbuffer be?
- presentation: how should that backbuffer be drawn into the window?

## Prerequisite Bridge

Lesson 16 answered the question:

```text
how do I draw this existing canvas inside a differently sized window?
```

Lesson 17 asks the bigger systems question:

```text
when the window changes size,
should the canvas itself stay fixed or change too?
```

That is where scale modes come in.

## Why This Lesson Exists

Letterbox math alone does not define resize behavior.

The runtime still needs a policy for what the backbuffer itself should do when the real window changes size.

This lesson makes that policy explicit and then shows how both backends route real resize events into the same shared decision.

## Step 1: Read the Shared Enum as Policy, Not Labels

In `src/utils/base.h`:

```c
typedef enum {
  WINDOW_SCALE_MODE_FIXED,
  WINDOW_SCALE_MODE_DYNAMIC_MATCH,
  WINDOW_SCALE_MODE_DYNAMIC_ASPECT,
  WINDOW_SCALE_MODE_COUNT
} WindowScaleMode;
```

These are not cosmetic names.

They are the runtime's resize-policy options.

## Step 2: Translate the Modes Into Plain-Language Stories

### `WINDOW_SCALE_MODE_FIXED`

```text
keep the backbuffer at GAME_W x GAME_H
only change how it is presented inside the window
```

### `WINDOW_SCALE_MODE_DYNAMIC_MATCH`

```text
resize the backbuffer to exactly match the real window size
```

### `WINDOW_SCALE_MODE_DYNAMIC_ASPECT`

```text
resize the backbuffer too,
but keep it on a chosen aspect ratio such as 4:3
```

This is the cleanest way to remember the modes: as three resize stories.

## Visual: The Three Resize Stories

```text
fixed:
  canvas stays fixed
  only presentation placement changes

dynamic match:
  canvas becomes the window size

dynamic aspect:
  canvas changes too,
  but only to a size that preserves the target aspect
```

## Step 3: Read `apply_scale_mode(...)` as the Policy Switch

Both backends implement the same decision logic.

From the live code:

```c
switch (props->scale_mode) {
case WINDOW_SCALE_MODE_FIXED:
  new_w = GAME_W;
  new_h = GAME_H;
  break;
case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
  new_w = win_w;
  new_h = win_h;
  break;
case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
  platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
  break;
default:
  new_w = GAME_W;
  new_h = GAME_H;
  break;
}
```

This means the essential policy job is:

```text
given the current mode and current window size,
choose the target backbuffer dimensions
```

Everything else follows from that one decision.

## Worked Example: One Resize Under All Three Modes

Suppose the real window changes from `800 x 600` to `1200 x 700`.

Then the backbuffer target becomes:

```text
fixed          -> 800 x 600
dynamic match  -> 1200 x 700
dynamic aspect -> fit a 4:3 canvas inside 1200 x 700
```

That comparison is the fastest way to feel the policy difference.

## Step 4: A Policy Decision Becomes a Real Memory Decision

After choosing `new_w` and `new_h`, both backends do the same shared check:

```c
if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
  backbuffer_resize(&props->backbuffer, new_w, new_h);
  ... backend-specific GPU update ...
}
```

This line is crucial.

It means scale mode is not merely about presentation cosmetics.

It can cause the software-render target itself to be reallocated.

## Step 5: Read `backbuffer_resize(...)` as Contract Repair

The shared resize helper is:

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

This is the same contract idea you learned in Module 03:

```text
if the image shape changes,
both memory and metadata must continue to agree
```

So resize policy eventually becomes a concrete memory-layout update.

## Step 6: Shared Policy and Backend GPU Consequences Are Different Layers

After `backbuffer_resize(...)`, the backends diverge.

### Raylib consequence

Raylib cannot resize a `Texture2D` in place, so it recreates the texture:

```c
UnloadTexture(g_raylib.texture);
Image img = {
    .data = props->backbuffer.pixels,
    .width = new_w,
    .height = new_h,
    .mipmaps = 1,
    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
};
g_raylib.texture = LoadTextureFromImage(img);
```

### X11 consequence

X11 keeps the texture object but reallocates its GPU storage:

```c
glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_w, new_h, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, NULL);
```

So the layering is very clean:

```text
shared code chooses backbuffer policy
shared code resizes the software image
backend code repairs GPU-side presentation resources
```

## Step 7: Real Resize Events Feed Into the Same Policy Hook

The event source differs by backend, but both route into `apply_scale_mode(...)`.

### Raylib

```c
if (IsWindowResized()) {
  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();
  apply_scale_mode(&props, win_w, win_h);
}
```

### X11

Inside `ConfigureNotify`:

```c
int new_w = ev.xconfigure.width;
int new_h = ev.xconfigure.height;
if (new_w != g_x11.window_w || new_h != g_x11.window_h) {
  g_x11.window_w = new_w;
  g_x11.window_h = new_h;
  apply_scale_mode(props, g_x11.window_w, g_x11.window_h);
}
```

So once again, the host API differs but the internal policy hook is shared.

## Step 8: Tab Cycling Is the Payoff of Edge-Triggered Input

The runtime maps Tab to `cycle_scale_mode`, then switches modes like this:

```c
if (button_just_pressed(curr_input->buttons.cycle_scale_mode)) {
  props.scale_mode =
      (WindowScaleMode)((props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT);
}
```

This is a direct payoff from Lessons 13 and 14.

Without edge-triggered input, holding Tab would rip through all the modes every frame.

With `button_just_pressed(...)`, one physical press means one policy change.

## Step 9: Keep Policy Separate From Presentation Placement

This lesson becomes much easier if you keep two questions separate.

### Question A

```text
what size should the backbuffer be?
```

Scale-mode policy answers this.

### Question B

```text
how should that backbuffer be drawn into the real window?
```

Letterbox and presentation helpers answer this.

In fixed mode, Question A usually yields `800 x 600`, while Question B decides bars and centering.

In dynamic modes, Question A may change the backbuffer shape itself.

## Worked Comparison: Why Fixed Mode Is Different

Suppose the window becomes `1400 x 800`.

### Fixed mode

```text
backbuffer stays 800 x 600
presentation computes scale + offsets
black bars or centered fit may appear
```

### Dynamic match

```text
backbuffer becomes 1400 x 800
presentation can often draw at 1:1 scale
```

### Dynamic aspect

```text
backbuffer becomes the largest 4:3 size that fits in 1400 x 800
presentation centers that resized backbuffer
```

That contrast is the whole lesson in compact form.

## Practical Exercises

### Exercise 1: Predict the Backbuffer Size

Window becomes `1280 x 720`.

What backbuffer size should each mode choose?

Expected structure:

```text
fixed          -> 800 x 600
dynamic match  -> 1280 x 720
dynamic aspect -> largest 4:3 size inside 1280 x 720
```

### Exercise 2: Explain the Layer Split

Why is `apply_scale_mode(...)` not the same thing as `display_backbuffer(...)`?

Expected answer:

```text
apply_scale_mode chooses or changes the backbuffer size;
display_backbuffer decides how the current backbuffer is presented in the window
```

### Exercise 3: Connect Input to Policy

Why is `button_just_pressed(...)` the right input helper for cycling scale modes?

Expected answer:

```text
because one physical Tab press should advance the mode once, not every frame while the key is held
```

## Common Mistakes

### Mistake 1: Thinking resize handling is only presentation math

In dynamic modes it can trigger real backbuffer reallocation.

### Mistake 2: Forgetting the backend still has GPU resource work to do after shared resize policy runs

Changing the software backbuffer size is only half of the job.

### Mistake 3: Mixing up fixed-mode letterboxing with dynamic-aspect resizing

One preserves an existing canvas during presentation; the other chooses a new canvas size first.

## Troubleshooting Your Understanding

### “I still mix up fixed mode and dynamic aspect”

Ask this question:

```text
does the backbuffer itself stay 800 x 600,
or does it get a new size that happens to preserve 4:3?
```

If it stays fixed, that is fixed mode.
If it gets a new aspect-preserving size, that is dynamic aspect.

### “Why does the course emphasize shared policy so much?”

Because if resize behavior differs in meaning across backends, the runtime stops being portable in any useful sense.

Shared policy is what keeps Raylib and X11 teaching the same architecture.

## Recap

You now understand the full resize-policy layer:

1. `WindowScaleMode` defines three distinct resize stories
2. `apply_scale_mode(...)` converts window size and policy into target backbuffer dimensions
3. dynamic modes can trigger real `backbuffer_resize(...)` memory changes
4. backend code must then repair GPU presentation resources
5. resize events and Tab input both feed into the same shared policy hook
6. presentation math and resize policy are related but not the same layer

## Module Recap

Module 04 has now made one frame trustworthy in four different ways:

- impossible internal states can fail fast
- buttons mean something precise for one frame
- whole-frame input snapshots are assembled cleanly
- time and resize behavior are explicit runtime policies instead of hidden side effects

That is the control-system foundation the rest of the course will build on.
