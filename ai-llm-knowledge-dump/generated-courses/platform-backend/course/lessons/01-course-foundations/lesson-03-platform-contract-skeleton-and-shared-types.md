# Lesson 03: Platform Contract, Shared Resource Bundle, and Startup Helpers

## Frontmatter

- Module: `01-course-foundations`
- Lesson: `03`
- Status: Required
- Target file:
  - `src/platform.h`
- Target symbols:
  - `STRINGIFY`
  - `TOSTRING`
  - `GAME_TITLE`
  - `GAME_VERSION`
  - `TITLE`
  - `ARRAY_LEN`
  - `PlatformGameProps`
  - `platform_game_props_init`
  - `platform_level_arena_reset`
  - `platform_game_props_free`
  - `platform_compute_letterbox`
  - `platform_compute_aspect_size`
  - `platform_audio_init`
  - `platform_audio_shutdown`
  - `platform_audio_get_samples_to_write`
  - `platform_audio_write`

## Observable Outcome

By the end of this lesson, you should be able to explain why `src/platform.h` exists, what `PlatformGameProps` owns, how `platform_game_props_init(...)` acquires resources, and why the cleanup path mirrors the setup path.

You should also be able to explain the difference between these two ideas:

```text
shared contract
```

and

```text
random dumping ground for code both backends happen to use
```

This file is the first one.

## Prerequisite Bridge

Lesson 01 established the live runtime spine.

Lesson 02 showed how the build script chooses one backend shell around a shared runtime.

This lesson explains the shared language those backend shells use after the program is built.

You do not need to understand arenas deeply yet.

For now, you only need to notice that the contract already makes memory lifetimes explicit very early.

## Why This Lesson Exists

As soon as a project has more than one backend, drift becomes a risk.

Without a clear contract:

- one backend allocates startup resources differently
- another picks different defaults
- cleanup stops mirroring setup
- game code starts depending on backend-specific details

`src/platform.h` exists to stop that drift.

Its job is simple:

```text
define the shared language
without swallowing the game logic
and without duplicating backend-specific implementation
```

## Step 1: Trust the Header Comment

The file begins by telling you what belongs here:

```c
/* This header contains ONLY:
 *   Types both sides see
 *   Platform-provided function prototypes
 *   Shared constants
 *   Utility macros
 *
 * It does NOT contain game logic. Game logic lives in game/base.h.
 */
```

This is not decorative commentary.

It is an architecture rule.

When reading a large codebase, comments like this are extremely valuable because they tell you what the author is trying to prevent.

Here, the prevention target is contract drift.

## Step 2: Read the Macro Block Carefully

Near the top of the file, we get this block:

```c
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef GAME_TITLE
#define GAME_TITLE "Platform Foundation"
#endif
#ifndef GAME_VERSION
#define GAME_VERSION "1.0"
#endif
#define TITLE GAME_TITLE " v" GAME_VERSION

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
```

Each line exists for a reason.

### `TARGET_FPS`

```c
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif
```

- Both backends need one shared default timing target.
- Putting it here prevents each backend from inventing its own default.

### `STRINGIFY` and `TOSTRING`

```c
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
```

This is a classic two-step C macro trick.

#### `STRINGIFY(x)`

- Turns the token `x` into a string literal.
- It does not first expand `x` if `x` is itself a macro.

#### `TOSTRING(x)`

- Forces expansion first.
- Then stringifies the expanded result.

### Worked micro-example

Suppose the code contains:

```c
#define GAME_VERSION "1.0"
```

Then these behave differently:

```text
STRINGIFY(GAME_VERSION) -> "GAME_VERSION"
TOSTRING(GAME_VERSION)  -> "1.0"
```

That difference is exactly why both macros exist.

### `TITLE`

```c
#define TITLE GAME_TITLE " v" GAME_VERSION
```

- Produces one shared compile-time title string.
- Ensures both backends display the same app identity.

### `ARRAY_LEN`

```c
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
```

This works for real arrays whose size is known in the current scope.

It does not work for pointers.

This is an excellent place to reinforce a web-developer transition rule:

```text
C pointers do not carry length metadata.
```

If you forget that, you will misread a lot of low-level code.

## Step 3: Notice the Include Surface

The shared header includes:

```c
#include "./utils/audio.h"
#include "./utils/backbuffer.h"
#include "./utils/vec3.h"
#include "./utils/base.h"
#include "./utils/arena.h"
```

This tells us what the contract depends on.

### What matters immediately

- `audio.h` exposes shared audio types.
- `backbuffer.h` exposes the CPU-side render target type.
- `base.h` carries shared game/platform constants and enums.
- `arena.h` exposes the allocator types used at the contract boundary.

### What to notice but not over-focus on yet

- `vec3.h` is present in the contract surface.
- The current course runtime is still fundamentally 2D in the early modules.
- So treat this as future-scope support material, not your main current concern.

This is an important reading skill:

```text
notice every dependency
but do not give every dependency equal weight
```

## Step 4: Read `PlatformGameProps` as an Ownership Map

This is the central shared resource bundle:

```c
typedef struct {
  Backbuffer backbuffer;
  AudioOutputBuffer audio_buffer;
  PlatformAudioConfig audio_config;
  WindowScaleMode scale_mode;
  GameWorldConfig world_config;
  Arena perm;
  Arena level;
  Arena scratch;
} PlatformGameProps;
```

Do not read this as "a struct with fields."

Read it as "the list of resources that every backend must initialize the same way."

### `Backbuffer backbuffer`

- CPU-side pixel memory.
- Both backends eventually display this same logical image.

### `AudioOutputBuffer audio_buffer`

- Shared sample buffer the game writes into.
- The backend later pushes those samples to a real audio device.

### `PlatformAudioConfig audio_config`

- Shared device/stream settings.
- Keeps audio setup language consistent across backends.

### `WindowScaleMode scale_mode`

- Shared presentation policy.
- Prevents the backends from inventing different meanings for scaling behavior.

### `GameWorldConfig world_config`

- Shared coordinate and camera defaults.
- Again, this protects meaning from drifting.

### `Arena perm`, `Arena level`, `Arena scratch`

Even before the allocator lessons, the names tell a story:

- `perm` means session lifetime
- `level` means scene or reload lifetime
- `scratch` means very short temporary lifetime

That means the runtime is built around explicit lifetimes, not just "allocate some memory somewhere."

## Visual: Ownership Summary

```text
PlatformGameProps
  |
  +-- backbuffer    CPU pixel memory
  +-- audio_buffer  shared sample memory
  +-- audio_config  audio device configuration data
  +-- scale_mode    presentation policy
  +-- world_config  coordinate and camera defaults
  +-- perm          session lifetime arena
  +-- level         scene lifetime arena
  +-- scratch       temporary frame/work arena
```

If you understand this struct as an ownership map, you already understand a big part of the backend architecture.

## Step 5: Walk the Success Path of `platform_game_props_init(...)`

Here is the high-level shape of the initializer:

```text
1. set shared defaults
2. allocate backbuffer
3. configure audio settings
4. allocate audio sample buffer
5. bootstrap perm arena
6. bootstrap level arena
7. bootstrap scratch arena
8. return success
```

Always understand the success path first.

Then study the failure-unwind path.

## Step 6: Shared Defaults Come First

The initializer begins like this:

```c
props->scale_mode = WINDOW_SCALE_MODE_FIXED;
props->world_config.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;
props->world_config.camera_zoom = 1.0f;
props->world_config.camera_x = 0.0f;
props->world_config.camera_y = 0.0f;
```

This block is not random startup noise.

It establishes one deterministic presentation state for every backend.

### Why this matters

If one backend defaulted to a different scale mode or coordinate origin, the same game code could look wrong for reasons that are hard to see.

The contract prevents that class of bug.

### Key detail: `camera_zoom = 1.0f`

This is especially worth noticing.

`1.0f` means neutral zoom.

`0.0f` would not mean "no zoom."

It would mean a broken or degenerate view transform.

## Step 7: Backbuffer Allocation

Next comes the CPU pixel buffer setup:

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

### What this is doing

- Width and height come from shared game constants.
- `4` bytes per pixel means 32-bit pixels.
- `pitch` is the byte distance from one row to the next.
- The pixel memory is explicitly allocated.
- `memset` clears it to black or zeroed data.

### Beginner pitfall

`pitch` is not the same thing as width.

It is measured in bytes, not pixels.

This matters later when iterating row-by-row.

## Step 8: Audio Configuration and Buffer Allocation

Then the initializer sets audio defaults:

```c
props->audio_config.sample_rate = AUDIO_SAMPLE_RATE;
props->audio_config.channels = AUDIO_CHANNELS;
props->audio_config.chunk_size = AUDIO_CHUNK_SIZE;
```

And then allocates the sample buffer:

```c
props->audio_buffer.samples_per_second = AUDIO_SAMPLE_RATE;
props->audio_buffer.sample_count = AUDIO_CHUNK_SIZE;
props->audio_buffer.max_sample_count = AUDIO_CHUNK_SIZE;
props->audio_buffer.format = AUDIO_FORMAT_F32;
props->audio_buffer.is_initialized = 0;

{
  int max_bps = audio_format_bytes_per_sample(AUDIO_FORMAT_F64);
  props->audio_buffer.samples_buffer =
      malloc((size_t)AUDIO_CHUNK_SIZE * (size_t)max_bps);
  if (!props->audio_buffer.samples_buffer) {
    free(props->backbuffer.pixels);
    props->backbuffer.pixels = NULL;
    return -1;
  }
  memset(props->audio_buffer.samples_buffer, 0,
         (size_t)AUDIO_CHUNK_SIZE * (size_t)max_bps);
}
```

### Why allocate for `AUDIO_FORMAT_F64`?

Because the code wants enough storage for the largest supported sample format.

That means the buffer remains safe even if the backend chooses a different concrete format later.

This is a contract-level safety choice.

## Step 9: Arena Bootstrap Makes Lifetimes Explicit

After pixels and audio, the initializer bootstraps three arenas:

```c
if (arena_bootstrap(&props->perm, ARENA_PERM_SIZE, ARENA_PERM_SIZE,
                    ARENA_LIFETIME_SESSION, "perm") != 0) {
  ...
}

if (arena_bootstrap(&props->level, ARENA_LEVEL_SIZE, ARENA_LEVEL_SIZE,
                    ARENA_LIFETIME_LEVEL, "level") != 0) {
  ...
}

if (arena_bootstrap(&props->scratch, ARENA_SCRATCH_SIZE, ARENA_SCRATCH_SIZE,
                    ARENA_LIFETIME_FRAME, "scratch") != 0) {
  ...
}
```

You do not need the allocator internals yet.

For now, focus on the naming:

- `perm` is session lifetime
- `level` is scene lifetime
- `scratch` is temporary lifetime

That tells you the runtime treats memory lifetime as a first-class design concern.

## Step 10: Failure Unwind Mirrors Success

One of the best things in this function is the cleanup logic on failure.

Example:

```c
if (!props->audio_buffer.samples_buffer) {
  free(props->backbuffer.pixels);
  props->backbuffer.pixels = NULL;
  return -1;
}
```

Later:

```c
if (arena_bootstrap(&props->level, ...) != 0) {
  arena_free(&props->perm);
  free(props->audio_buffer.samples_buffer);
  props->audio_buffer.samples_buffer = NULL;
  free(props->backbuffer.pixels);
  props->backbuffer.pixels = NULL;
  return -1;
}
```

This is the rule you should learn:

```text
every successful acquisition creates a cleanup obligation
```

So the failure path walks backward, releasing everything that was already acquired.

That is professional systems programming discipline.

## Step 11: `platform_level_arena_reset(...)`

The next helper is short but important:

```c
static inline void platform_level_arena_reset(PlatformGameProps *props) {
  arena_reset(&props->level);
}
```

This function communicates a lifetime policy:

```text
scene-lifetime allocations can be cleared together
without destroying session-lifetime state
```

Even before the allocator module, that is a very powerful architectural clue.

## Step 12: `platform_game_props_free(...)`

The full cleanup helper releases everything in the reverse direction:

```c
arena_free(&props->scratch);
arena_free(&props->level);
arena_free(&props->perm);

if (props->audio_buffer.samples_buffer) {
  free(props->audio_buffer.samples_buffer);
  props->audio_buffer.samples_buffer = NULL;
}
if (props->backbuffer.pixels) {
  free(props->backbuffer.pixels);
  props->backbuffer.pixels = NULL;
}
```

This matters for the same reason the failure path matters:

```text
resource ownership is only trustworthy if cleanup is systematic
```

In debug builds, the function also dumps arena stats before freeing, which turns cleanup into an observability opportunity.

## Step 13: Letterbox Math Helper

The contract also provides a shared presentation helper:

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

The algorithm is:

```text
scale = min(window_w / canvas_w, window_h / canvas_h)
offset_x = center leftover horizontal space
offset_y = center leftover vertical space
```

If you come from CSS, the closest analogy is:

```text
object-fit: contain
```

But here you compute it manually instead of letting the browser do it.

## Step 14: Aspect-Preserving Size Helper

The next helper computes a canvas size that preserves a target aspect ratio:

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

This helper answers a different question than letterboxing.

Letterboxing says:

```text
given a fixed canvas, how do I fit it inside a window?
```

Aspect-size says:

```text
given a window, what canvas size should I use to preserve my target aspect ratio?
```

That difference matters later when scale modes become more sophisticated.

## Step 15: Audio Prototypes Define the Platform Edge

At the bottom of the file we get platform-provided audio function prototypes:

```c
int platform_audio_init(PlatformAudioConfig *config);
void platform_audio_shutdown(void);
int platform_audio_get_samples_to_write(PlatformAudioConfig *config);
void platform_audio_write(AudioOutputBuffer *buffer, int num_frames,
                          PlatformAudioConfig *config);
```

These do something subtle but important.

They define what the game layer is allowed to assume about platform audio support without exposing the backend implementation details.

### Why this is a good contract boundary

- The game layer can ask for sample production in shared types.
- The backend can implement these functions differently on X11 and Raylib.
- The shared runtime does not need to know ALSA or Raylib device details.

This is the same architectural pattern we saw in Lesson 01 at the larger application level.

## Worked Example: Why `platform.h` Exists

Imagine there were no shared contract.

Then the X11 backend might choose one default coordinate origin, while the Raylib backend chooses another.

One backend might allocate a different audio buffer size.

One backend might forget to initialize camera zoom.

The game layer would appear inconsistent even though its own logic had not changed.

`src/platform.h` prevents that by centralizing the shared rules.

## Common Beginner Mistakes

### Mistake 1: Thinking `PlatformGameProps` is "just a convenience struct"

It is more than convenience.

It is an ownership and initialization policy shared across backends.

### Mistake 2: Not reading failure cleanup as part of the design

The cleanup logic is not secondary.

In systems programming, the cleanup path is part of the architecture.

### Mistake 3: Assuming `ARRAY_LEN` works on pointers

It does not.

Only real arrays known in the current scope have compile-time size information.

### Mistake 4: Treating every helper as equally important right now

Some pieces are immediate.

Some are future-facing.

You should notice all of them, but not give them all equal cognitive weight on day one.

## Practice

Answer these before moving on:

1. Why is `camera_zoom = 1.0f` a meaningful default instead of `0.0f`?
2. Why does the contract allocate the audio sample buffer for the largest supported format size?
3. Why is `platform_compute_letterbox(...)` a different kind of helper than `platform_compute_aspect_size(...)`?
4. Why is the failure-unwind path in `platform_game_props_init(...)` just as important as the success path?

## Mini Exercises

### Exercise 1

Without looking back, write a one-sentence description of each arena:

- `perm`
- `level`
- `scratch`

You do not need allocator internals yet. Just describe the lifetime intention.

### Exercise 2

Suppose backbuffer allocation succeeds but audio buffer allocation fails.

Which resource must be cleaned up before returning?

Explain why that cleanup obligation exists.

### Exercise 3

Explain the difference between these two questions:

```text
How do I fit this canvas inside the window?
What canvas size should I use to preserve this aspect ratio?
```

Map the first to `platform_compute_letterbox(...)` and the second to `platform_compute_aspect_size(...)`.

## Troubleshooting Your Understanding

### "I do not yet understand arenas"

That is okay.

For now, you only need the lifetime picture:

```text
perm    = long-lived
level   = reset on scene reload
scratch = temporary work memory
```

That is enough for this lesson.

### "I keep thinking the backend should own all initialization itself"

The backend still owns backend-specific initialization.

But shared resource policy belongs in the shared contract so both backends stay aligned.

### "The helper functions feel random"

They are not random.

They are the pieces of platform-side logic whose meaning must stay consistent across backends.

## Recap

You learned that `src/platform.h` is responsible for:

1. Defining shared macros and constants.
2. Defining the shared resource bundle in `PlatformGameProps`.
3. Centralizing startup defaults.
4. Centralizing resource acquisition and cleanup policy.
5. Providing shared presentation math helpers.
6. Declaring the platform audio edge used by both backends.

That is why this file is a contract and not a dumping ground.

## End of Module 01

At this point you have a stable mental model of the opening architecture:

```text
Lesson 01: where the live runtime path is
Lesson 02: what build identity gets compiled
Lesson 03: what shared contract the backends speak
```

That is enough foundation to start reading frame flow, input, timing, and rendering without getting lost immediately.

## Next Module Bridge

The next module can finally start following one frame through the runtime.

You now know:

- where the program comes from
- which shell runs it
- which shared contract stabilizes it

That is exactly the footing you need before studying input snapshots, frame pacing, and presentation flow.
