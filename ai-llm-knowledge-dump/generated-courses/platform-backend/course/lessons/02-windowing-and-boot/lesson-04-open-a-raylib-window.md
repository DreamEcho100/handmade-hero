# Lesson 04: Raylib Boot Path, Shared Startup, and the First Frame Shell

## Frontmatter

- Module: `02-windowing-and-boot`
- Lesson: `04`
- Status: Required
- Target files:
  - `src/platforms/raylib/main.c`
  - `src/platform.h`
- Target symbols:
  - `RaylibState`
  - `main`
  - `platform_game_props_init`
  - `platform_game_props_free`
  - `LoadTextureFromImage`
  - `init_audio`
  - `display_backbuffer`

## Observable Outcome

By the end of this lesson, you should be able to trace the Raylib backend from the first window call to the live frame loop and explain when the program gains each of these things:

- a real OS window
- the shared runtime bundle in `PlatformGameProps`
- a GPU texture that presents the CPU backbuffer
- optional audio stream state
- a live `GameAppState`
- a cleanup path that mirrors startup

If you can explain that sequence cleanly, you understand the smallest complete backend shell in the course.

## Prerequisite Bridge

Module 01 established the architecture vocabulary:

```text
build script
backend shell
shared contract
game facade
```

This lesson starts at the backend shell because that is where the runtime first touches the real machine.

Raylib comes first for a practical reason: it shows the entire boot shape without forcing you to learn X11 and GLX at the same time.

## Why This Lesson Exists

Raylib is not the "fake" backend.

It is the smaller backend shell.

That distinction matters.

Raylib still has to do all of the real platform-facing work that a backend shell must do:

- create a window
- allocate shared runtime resources
- create a presentation texture
- start audio if available
- initialize the shared game facade
- run the frame loop
- tear everything down correctly

The only thing Raylib hides is some of the lower-level API volume.

## The Whole Boot Shape First

Before reading any individual call, hold this startup map in your head:

```text
SetTraceLogLevel
  -> InitWindow
  -> SetTargetFPS
  -> platform_game_props_init
  -> LoadTextureFromImage
  -> init_audio
  -> game_app_init
  -> main loop
       -> input snapshot
       -> update
       -> audio drain
       -> render
       -> display_backbuffer
  -> cleanup in reverse order
```

The rest of the lesson is just unpacking that sequence carefully.

## Step 1: Read `RaylibState` as Backend-Owned State

The Raylib-specific state is:

```c
typedef struct {
  Texture2D texture;
  AudioStream audio_stream;
  int buffer_size_frames;
  int audio_initialized;
  int window_focused;
  int audio_paused;
  int prev_win_w;
  int prev_win_h;
} RaylibState;

static RaylibState g_raylib = {0};
```

This struct is not the runtime.

It is the backend's private platform-facing ledger.

### What each field category means

```text
presentation handle
  -> texture

audio handle and bookkeeping
  -> audio_stream, buffer_size_frames, audio_initialized, audio_paused

window/focus bookkeeping
  -> window_focused, prev_win_w, prev_win_h
```

### Important distinction

`RaylibState` does not replace `PlatformGameProps`.

It lives beside it.

The backend owns Raylib handles.
The shared contract owns the common runtime bundle.

## Visual: Shared Bundle vs Backend Bundle

```text
PlatformGameProps
  -> backbuffer
  -> audio buffer
  -> audio config
  -> world config
  -> perm / level / scratch

RaylibState
  -> Texture2D
  -> AudioStream
  -> focus and pause state
  -> cached window size
```

That split is the main architectural lesson of the file.

## Step 2: Window Creation Happens Before Shared Runtime Startup

At the top of `main()` the backend does this:

```c
SetTraceLogLevel(LOG_WARNING);
InitWindow(GAME_W, GAME_H, TITLE);
SetTargetFPS(TARGET_FPS);

if (!IsWindowReady()) {
  fprintf(stderr, "Raylib: InitWindow failed\n");
  return 1;
}

SetWindowState(FLAG_WINDOW_RESIZABLE);
```

### What exists after `InitWindow(...)`?

```text
a real OS window exists
but the shared runtime bundle does not exist yet
```

That ordering matters.

The backend first claims a platform resource.
Only after that does it ask shared startup to claim runtime resources.

### Why `TITLE` matters here

The window title does not come from a Raylib-only string literal.

It comes from the shared contract in `src/platform.h`.

That is a small but important sign that even the backend boot path is already aligned with the shared contract.

## Step 3: Shared Startup Claims the Runtime Bundle

Next comes the common resource bundle:

```c
PlatformGameProps props = {0};
if (platform_game_props_init(&props) != 0) {
  fprintf(stderr, "Out of memory\n");
  CloseWindow();
  return 1;
}
```

This step is where the backend shell stops being just a window and starts becoming the host for the shared runtime.

### What `platform_game_props_init(...)` gives us

From earlier lessons, this shared initializer claims:

- the CPU backbuffer
- the audio sample buffer
- the audio configuration struct
- presentation defaults
- `perm`, `level`, and `scratch` arenas

### Failure unwind lesson

If shared startup fails, the already-created window must still be cleaned up.

That is why the code calls `CloseWindow()` before returning.

Even in the smaller backend, startup and cleanup are already paired.

## Step 4: The CPU Backbuffer Becomes a GPU Texture

Once `props.backbuffer.pixels` exists, the backend creates the presentation bridge:

```c
Image img = {
    .data = props.backbuffer.pixels,
    .width = GAME_W,
    .height = GAME_H,
    .mipmaps = 1,
    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
};
g_raylib.texture = LoadTextureFromImage(img);
SetTextureFilter(g_raylib.texture, TEXTURE_FILTER_POINT);

if (!IsTextureValid(g_raylib.texture)) {
  fprintf(stderr, "Raylib: LoadTextureFromImage failed\n");
  platform_game_props_free(&props);
  CloseWindow();
  return 1;
}
```

### What this means conceptually

```text
shared startup gave us CPU pixel memory
Raylib now gives us a GPU object that can display it
```

### Why `TEXTURE_FILTER_POINT` matters

The course is using a software-rendered pixel image.

Nearest-neighbor filtering keeps those pixels crisp instead of blurred.

### Failure unwind lesson again

If texture creation fails:

- the shared runtime bundle must be freed
- the window must be closed

That reverse-order cleanup is not optional.

## Step 5: Audio Is Optional Backend Capability, Not Core Startup Identity

The backend then does:

```c
if (init_audio(&props.audio_config, &props.audio_buffer) != 0) {
  fprintf(stderr, "Audio init failed - continuing without audio\n");
}
```

This teaches an important boot policy.

The course treats audio device startup as desirable but non-fatal.

That means the runtime is allowed to keep running visually even if audio device setup fails.

This is a different kind of startup dependency than the window or the shared runtime bundle.

## Step 6: Game Facade Startup Is the Last Major Boot Step

Only after the window, shared bundle, texture bridge, and optional audio path exist does the backend create the shared game runtime:

```c
GameAppState *game = NULL;
if (game_app_init(&game, &props.perm) != 0) {
  fprintf(stderr, "Failed to initialize game facade\n");
  shutdown_audio();
  if (IsTextureValid(g_raylib.texture))
    UnloadTexture(g_raylib.texture);
  platform_game_props_free(&props);
  CloseWindow();
  return 1;
}
```

This is the first moment where the full runtime is actually ready.

Before this point, the backend only had resources.

After this point, it has a live game facade with state and behavior.

## Visual: What Exists After Each Boot Stage?

```text
after InitWindow:
  window only

after platform_game_props_init:
  window + shared runtime bundle

after LoadTextureFromImage:
  window + shared runtime bundle + GPU presentation texture

after init_audio:
  maybe audio stream too

after game_app_init:
  full runtime ready to enter frame loop
```

## Step 7: The Frame Loop Is a Repeated Ownership Pattern

Once boot completes, the loop repeatedly does this:

```text
swap and prepare input snapshots
poll input into the current snapshot
run game update
process audio if available
open frame scratch scope
run game render into the backbuffer
display the backbuffer
```

That is not just a sequence of function calls.

It is the contract between backend shell and shared runtime.

The backend provides:

- input polling
- timing source
- audio device handling
- presentation path

The game facade provides:

- update behavior
- rendering into the backbuffer
- audio sample generation

## Step 8: `display_backbuffer(...)` Is the Presentation Edge

The display function starts like this:

```c
UpdateTexture(g_raylib.texture, backbuffer->pixels);
```

That one line is the core presentation bridge.

It says:

```text
take the latest CPU-rendered pixel memory
and copy it into the GPU texture
```

Then the backend decides where and how to draw that texture into the window.

Module 03 will break this down in much more detail, but the important boot-level fact is already visible here: the game does not draw straight to the monitor. It fills a CPU backbuffer, and the backend presents that image.

## Step 9: Cleanup Mirrors Startup Exactly

At shutdown, the backend tears down resources in the reverse order of dependence:

- game loop exits
- audio shuts down if initialized
- texture is unloaded if valid
- shared runtime bundle is freed
- window is closed

That gives us the real systems rule of the lesson:

```text
startup walks forward through dependencies
cleanup walks backward through dependencies
```

If you internalize that rule here, later allocator and audio lessons will make much more sense.

## Worked Example: Why Reverse Cleanup Is Correct

Suppose `game_app_init(...)` fails.

At that moment the backend already owns:

- a window
- the shared runtime bundle
- a valid texture
- maybe audio state

It does not own a usable game facade.

So cleanup must release everything that was already acquired, in the reverse order that made those later steps possible.

That is why the failure path is not just error handling. It is an ownership proof.

## Common Beginner Mistakes

### Mistake 1: Thinking Raylib owns the runtime

It does not.

Raylib hosts the shared runtime.

### Mistake 2: Treating `RaylibState` like the common app state

`RaylibState` is backend-private bookkeeping.

`PlatformGameProps` and `GameAppState` are the more important runtime structures.

### Mistake 3: Looking only at startup success and ignoring failure unwind

In systems code, the failure path often teaches ownership more clearly than the happy path.

## Practice

Answer these before moving on:

1. What exists after `InitWindow(...)` that does not yet exist after `game_app_init(...)`?
2. Why is `platform_game_props_init(...)` not replaced by `RaylibState`?
3. Why is audio initialization allowed to fail without killing the whole backend?
4. Why must texture creation failure call both `platform_game_props_free(...)` and `CloseWindow()`?

## Mini Exercise

Without looking back, list the startup stages in order from first platform resource to first frame-loop iteration.

If you miss a stage, reread Steps 2 through 7.

## Troubleshooting Your Understanding

### "I still feel like too much happens in `main()`"

Compress it into five nouns:

```text
window
shared bundle
texture bridge
game facade
frame loop
```

That is the real shape.

### "I keep mixing up CPU backbuffer and GPU texture"

The CPU backbuffer lives in `PlatformGameProps`.

The GPU texture lives in `RaylibState`.

They are related, but they are not the same thing.

## Recap

This lesson established the smallest complete backend shell in the course:

1. Raylib creates the window.
2. Shared startup creates the common runtime bundle.
3. Raylib creates the presentation texture.
4. Audio tries to initialize as an optional backend capability.
5. The game facade starts.
6. The frame loop repeatedly bridges backend work to shared runtime work.
7. Cleanup runs in reverse order.

That is the full boot story in its simplest form.

## Next Lesson

Lesson 05 keeps the same architecture but removes most of Raylib's convenience.

The question becomes:

```text
what exact resources does the backend have to claim by hand
when Raylib is no longer hiding the window-system details?
```
