# Lesson 06: Shared Startup, Backbuffer Ownership, and Cleanup Symmetry

## Frontmatter

- Module: `02-windowing-and-boot`
- Lesson: `06`
- Status: Required
- Target files:
  - `src/platform.h`
  - `src/utils/backbuffer.h`
  - `src/utils/backbuffer.c`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `PlatformGameProps`
  - `platform_game_props_init`
  - `platform_game_props_free`
  - `Backbuffer`
  - `backbuffer_resize`

## Observable Outcome

By the end of this lesson, you should be able to explain why Raylib and X11 still feel like one runtime instead of two separate programs:

- both backends allocate the same shared resource bundle
- both backends present the same CPU backbuffer through different platform shells
- both backends reuse the same shared cleanup logic for common runtime resources
- resize logic preserves a memory contract rather than only making the picture fit

This lesson is where the two backend stories collapse back into one ownership model.

## Prerequisite Bridge

Lessons 04 and 05 taught you two different backend shells.

This lesson reconnects them at the deeper level:

```text
if the shells differ,
what shared ownership model keeps them from drifting into different runtimes?
```

The answer is shared startup, shared backbuffer rules, and shared cleanup.

## Why This Lesson Exists

After reading both backends, the right beginner question is not:

```text
which backend is better?
```

The right question is:

```text
what shared ownership model makes them still feel like one program?
```

That question is much more important because the whole course depends on one consistent runtime underneath two platform shells.

## The Main Architecture Claim

This lesson is really defending one sentence:

```text
the backend shells differ at the edge,
but the runtime resources are shaped once
```

Everything else in the lesson is evidence for that claim.

## Step 1: Read `Backbuffer` as a Shared Memory Contract

The shared type is:

```c
typedef struct {
  uint32_t *pixels;
  int width;
  int height;
  int pitch;
  int bytes_per_pixel;
} Backbuffer;
```

This is intentionally plain.

There is no Raylib handle here.
There is no X11 handle here.
There is no GL object here.

It is just:

```text
pointer + shape + stride + pixel-size metadata
```

That simplicity is exactly why it can be shared across both backends.

## Visual: What the Shared Backbuffer Means

```text
Backbuffer
  pixels -----------> row 0 pixel data
                      row 1 pixel data
                      row 2 pixel data
                      ...

  width  = pixels per logical row
  height = number of logical rows
  pitch  = bytes per row
  bpp    = bytes per pixel
```

The important correction is:

```text
width is not pitch
pitch is not width
```

You need both.

## Step 2: Shared Startup Owns the First Real Shape of the Backbuffer

Inside `platform_game_props_init(...)`, shared startup sets the backbuffer like this:

```c
props->backbuffer.width = GAME_W;
props->backbuffer.height = GAME_H;
props->backbuffer.bytes_per_pixel = 4;
props->backbuffer.pitch = GAME_W * 4;
props->backbuffer.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
if (!props->backbuffer.pixels)
  return -1;
memset(props->backbuffer.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);
```

This is the first strong proof that the runtime is shared where it matters.

Neither backend invents its own private backbuffer contract.

Shared startup shapes it once.

## Step 3: Both Backends Call the Same Shared Initializer

Raylib does:

```c
PlatformGameProps props = {0};
if (platform_game_props_init(&props) != 0) {
  fprintf(stderr, "Out of memory\n");
  CloseWindow();
  return 1;
}
```

X11 does:

```c
PlatformGameProps props = {0};
if (platform_game_props_init(&props) != 0) {
  fprintf(stderr, "Out of memory\n");
  return 1;
}
```

That sameness is not small convenience.

It is the architecture holding the two shells together.

## Visual: Two Shells, One Shared Resource Claim

```text
raylib/main.c             x11/main.c
     |                        |
     +-----------+------------+
                 |
                 v
      platform_game_props_init(&props)
                 |
     +-----------+-----------+-----------+
     |           |           |           |
     v           v           v           v
 backbuffer   audio buf   world config   perm/level/scratch
```

That diagram is the whole point of the lesson.

## Step 4: `backbuffer_resize(...)` Preserves the Contract When Shape Changes

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

Beginners often see `realloc(...)` and think that is the whole story.

It is not.

The real success condition is:

```text
the memory shape changed
and all metadata that describes that shape changed with it
```

That is contract maintenance, not just resize convenience.

## Worked Example: Why `pitch` Must Be Updated Too

Suppose the backbuffer changes from `800x600` to `1024x768`.

If the code updated width and height but forgot to update pitch, row-walking code would still step through memory as if each row were `800 * 4 = 3200` bytes wide.

That would make pixel iteration wrong immediately.

So `backbuffer_resize(...)` is preserving the integrity of the memory contract, not merely resizing storage.

## Step 5: Each Backend Owns Presentation, Not the Backbuffer Contract

After resizing or updating the backbuffer, each backend adapts its own presentation path.

### Raylib

Raylib recreates or updates a `Texture2D`.

### X11/OpenGL

X11 reallocates or updates texture storage with GL calls.

### Shared rule

```text
shared code owns the CPU backbuffer contract
backend code owns how that contract reaches the screen
```

That is the clean split.

## Step 6: Shared Cleanup Is the Mirror of Shared Startup

Both backends eventually call:

```c
platform_game_props_free(&props);
```

That shared cleanup releases:

- scratch arena
- level arena
- perm arena
- audio sample memory
- backbuffer pixels

This matters because the common runtime resources should not have two separate destruction policies.

If they did, the shells would drift.

## Visual: Shared Startup and Shared Teardown

```text
startup:
  platform_game_props_init
    -> shape shared backbuffer
    -> shape shared audio buffer
    -> set shared config defaults
    -> bootstrap shared arenas

shutdown:
  platform_game_props_free
    -> free shared arenas
    -> free shared buffers
```

This symmetry is one of the healthiest architectural patterns in the course so far.

## Step 7: Read Startup and Cleanup as a Pair

This lesson is teaching a habit you should keep for the entire course:

```text
every time you read acquisition,
look for the matching release
```

That habit is especially important in:

- audio device handling
- allocator-backed scene state
- level reload flow
- runtime instrumentation that owns temporary strings or logs

The pairing pattern starts here.

## Worked Example: Why the Backends Still Feel Like One Program

Imagine a broken design where Raylib allocated one backbuffer struct, X11 allocated a different one, and each backend maintained its own resize rules and cleanup policy.

The course would stop feeling like one runtime with two shells.

It would start feeling like two separate engine branches that only happen to share some naming.

The current design avoids that by centralizing the runtime resource contract in shared startup and shared cleanup.

## Common Beginner Mistakes

### Mistake 1: Thinking `Backbuffer` is a graphics API object

It is not.

It is plain CPU memory plus metadata.

### Mistake 2: Treating resize as only a visual concern

Resize is also a memory-contract concern because width, height, pitch, and storage all have to stay in sync.

### Mistake 3: Ignoring shared cleanup because the backend-specific cleanup is more visible

Shared cleanup is exactly what prevents the backend shells from diverging.

## Practice

Answer these before moving on:

1. Why is `Backbuffer` simple enough to be shared across both backends?
2. Why is `backbuffer_resize(...)` about metadata correctness, not just `realloc(...)`?
3. Why does shared cleanup matter just as much as shared startup?
4. What would drift look like if each backend owned its own private backbuffer contract?

## Mini Exercise

Write a two-column note.

Left column: backend-owned resources.
Right column: shared runtime resources.

Put `texture`, `display`, `window`, `backbuffer`, `audio buffer`, and `scratch arena` into the correct column.

## Troubleshooting Your Understanding

### "I understand the backbuffer, but I still do not feel the ownership split"

Ask this question:

```text
would both backends still need this resource in almost the same form?
```

If yes, it probably belongs in the shared runtime bundle.

If no, it probably belongs in backend-private state.

### "I keep focusing on whichever backend file I read last"

Return to the two-shells-one-bundle diagram above. That is the module anchor.

## Recap

This lesson proved the main ownership claim of Module 02:

1. Both backends boot different platform shells.
2. Both backends still allocate the same shared runtime bundle.
3. Both backends still present the same CPU backbuffer contract.
4. Both backends still rely on the same shared cleanup path.

That is why the course still has one runtime rather than two independent programs.

## Next Module Bridge

Module 02 answered this question:

```text
how does the runtime get a real window and a shared frame shell?
```

Module 03 answers the next one:

```text
what exactly is the CPU image that the backend is presenting every frame?
```

That means the next module will zoom from boot ownership into pixel memory, upload format, rasterization, clipping, and text.
