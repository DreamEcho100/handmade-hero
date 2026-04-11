# Lesson 01: Course Map, Live Runtime Spine, and Source Parity

## Frontmatter

- Module: `01-course-foundations`
- Lesson: `01`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
  - `src/platform.h`
  - `src/game/main.h`
  - `src/game/audio_demo.h`
- Target symbols:
  - `TARGET_FPS`
  - `PlatformGameProps`
  - `GameSceneID`
  - `game_app_init`
  - `game_app_update`
  - `game_app_render`
  - `game_app_get_audio_samples`
  - `game_audio_init_demo`
  - `game_get_audio_samples`

## Observable Outcome

By the end of this lesson, you should be able to explain the live runtime path of this course in one sentence:

```text
build-dev.sh chooses one backend shell,
both backend shells speak through src/platform.h,
and both call the same public game facade in src/game/main.h.
```

You should also be able to answer two beginner-critical questions without guessing:

1. Which files are part of the active runtime path?
2. Which files exist for teaching continuity but are not the top-level entry point anymore?

If you can answer those, the rest of the course stops feeling like a scavenger hunt.

## Prerequisite Bridge

You do not need to know X11, Raylib, ALSA, OpenGL, custom allocators, or even C very well yet.

You only need three safe ideas:

1. A build script decides what gets compiled.
2. A header can expose a public API without exposing all implementation details.
3. A repository can contain both live code and older teaching-support code at the same time.

If you come from web development, this lesson is the equivalent of answering these questions before opening a large app:

```text
Which file is the real app entry point?
Which files are framework glue?
Which files are old demos, helpers, or submodules?
```

## Why This Lesson Exists

Low-level repositories can feel harder than they really are because the learner gets lost before the technical material even starts.

The common failure pattern looks like this:

```text
there are many headers
there are multiple backends
there are support demos
everything looks equally important
so nothing feels trustworthy
```

This lesson fixes that first.

Its job is not to teach rendering or audio yet.

Its job is to give you a stable mental map so every later lesson lands in the right place.

## The First Rule of the Course

The active course root is:

```text
platform-backend/course/
```

So when a lesson says:

```text
src/platform.h
```

it means:

```text
course/src/platform.h
```

That sounds obvious, but it protects you from a real beginner mistake: opening nearby archive material or old support files and treating them as the active runtime path.

## Step 1: Start With the Directory-Level Map

Before reading code, reduce the repository to a role map:

```text
course/
  build-dev.sh
  src/
    game/
    platforms/
    utils/
  lessons/
```

Read those as responsibilities:

- `build-dev.sh` decides which program gets compiled.
- `src/platforms/` contains backend shells.
- `src/platform.h` is the shared platform/game contract.
- `src/game/main.h` is the public game facade both backends call.
- `src/game/main.c` contains the live runtime behind that facade.
- `lessons/` explains how to reason about the whole system.

## Step 2: Hold the Runtime Spine in Your Head

This is the picture you should keep for the rest of Module 01:

```text
build-dev.sh
    |
    v
chooses one backend shell
    |
    +----------------------------+
    |                            |
    v                            v
src/platforms/raylib/main.c   src/platforms/x11/main.c
    |                            |
    +-------------+--------------+
                  |
                  v
            src/platform.h
       shared constants, helpers,
       resource bundle, audio edge
                  |
                  v
            src/game/main.h
         public game lifecycle API
                  |
                  v
            src/game/main.c
      scenes, rendering, runtime flow
```

The key idea is not just that there are two backends.

The key idea is that they both wrap one shared runtime.

## Step 3: Prove the Spine With the Build Script

The build script contains the most important source-parity comment in the opening module:

```bash
DEMO_SRC="src/game/demo_explicit.c"

# NOTE: demo_explicit.c and audio_demo.c remain in the course as teaching
# material, but the live runtime now routes through game/main.c.
SOURCES="src/utils/backbuffer.c src/utils/draw-shapes.c src/utils/draw-text.c $DEMO_SRC src/game/audio_demo.c src/game/main.c"
```

This small snippet teaches several things at once.

### What `DEMO_SRC` means

```bash
DEMO_SRC="src/game/demo_explicit.c"
```

- That file still exists.
- It is still compiled.
- It still matters for teaching continuity.
- But it is not the top-level runtime facade.

### What `SOURCES` proves

```bash
SOURCES="... src/game/audio_demo.c src/game/main.c"
```

- The shared runtime core includes `src/game/main.c`.
- `src/game/audio_demo.c` is still a live subsystem.
- The build script explicitly tells you the runtime now routes through `game/main.c`.

That gives us the first non-negotiable conclusion:

```text
live front door = src/game/main.h / src/game/main.c
audio_demo = subsystem
demo_explicit = teaching continuity
```

This distinction matters because beginners often see several public-looking files and assume they are all equal.

They are not.

## Step 4: Read the Public Game Facade

Open `src/game/main.h` and focus on the public surface, not the hidden implementation:

```c
typedef enum {
  GAME_SCENE_ARENA_LAB = 0,
  GAME_SCENE_LEVEL_RELOAD,
  GAME_SCENE_POOL_LAB,
  GAME_SCENE_SLAB_AUDIO_LAB,
  GAME_SCENE_COUNT
} GameSceneID;

typedef struct GameAppState GameAppState;

int game_app_init(GameAppState **out_game, Arena *perm);
void game_app_update(GameAppState *game, GameInput *input, float dt,
                     Arena *level);
void game_app_render(GameAppState *game, Backbuffer *backbuffer, int fps,
                     WindowScaleMode scale_mode, GameWorldConfig world_config,
                     Arena *perm, Arena *level, Arena *scratch);
void game_app_get_audio_samples(GameAppState *game, AudioOutputBuffer *buf,
                                int num_frames);
```

This header is doing real architectural work.

### `GameSceneID`

```c
typedef enum {
  GAME_SCENE_ARENA_LAB = 0,
  GAME_SCENE_LEVEL_RELOAD,
  GAME_SCENE_POOL_LAB,
  GAME_SCENE_SLAB_AUDIO_LAB,
  GAME_SCENE_COUNT
} GameSceneID;
```

- The runtime currently has four real scenes.
- Those scene identities are public enough to matter at the application boundary.
- `GAME_SCENE_COUNT` is the size marker for the scene set.

If you come from React or Node, think of this like exporting a stable route or screen enum instead of scattering magic numbers everywhere.

### Forward declaration of `GameAppState`

```c
typedef struct GameAppState GameAppState;
```

This means:

- The backends are allowed to hold a `GameAppState *`.
- The backends are not allowed to know the whole struct layout here.
- The game runtime keeps its private internal state private.

This is the C version of exposing an interface without leaking the implementation object.

### `game_app_init`

```c
int game_app_init(GameAppState **out_game, Arena *perm);
```

- The backend asks the game runtime to boot.
- The backend supplies long-lived memory through `perm`.
- The runtime returns an opaque app-state pointer.

### `game_app_update`

```c
void game_app_update(GameAppState *game, GameInput *input, float dt,
                     Arena *level);
```

- The backend gathers input.
- The backend measures frame timing.
- The backend passes both into shared game logic.
- `level` already hints that the runtime distinguishes memory lifetimes.

### `game_app_render`

```c
void game_app_render(... Arena *perm, Arena *level, Arena *scratch);
```

Even before the allocator module, this tells you something important:

```text
the runtime is not built around one undifferentiated memory blob
```

There are multiple lifetimes, and the public API makes that explicit.

### `game_app_get_audio_samples`

```c
void game_app_get_audio_samples(GameAppState *game, AudioOutputBuffer *buf,
                                int num_frames);
```

- The platform/backend owns the actual audio device.
- The game layer owns the logic that fills the sample buffer.
- This is one of the cleanest separations in the whole project.

## Step 5: Prove Both Backends Use the Same Facade

It is not enough to see the header. We need proof that both backend shells actually call it.

### Raylib backend

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

...

game_app_update(game, curr_input, GetFrameTime(), &props.level);

...

game_app_render(game, &props.backbuffer, GetFPS(), props.scale_mode,
                props.world_config, &props.perm, &props.level,
                &props.scratch);
```

### X11 backend

```c
GameAppState *game = NULL;
if (game_app_init(&game, &props.perm) != 0) {
  fprintf(stderr, "Failed to initialize game facade\n");
  platform_audio_shutdown_cfg(&props.audio_config);
  shutdown_window();
  platform_game_props_free(&props);
  return 1;
}

...

game_app_update(game, curr_input, dt, &props.level);

...

game_app_render(game, &props.backbuffer, fps_display, props.scale_mode,
                props.world_config, &props.perm, &props.level,
                &props.scratch);
```

### What these snippets prove

The platform details differ.

The game lifecycle calls do not.

That means the architecture is:

```text
two platform shells
around one shared runtime
```

That is the central structural fact of the course.

## Step 6: Do Not Confuse the App Facade With a Subsystem API

Another beginner mistake is treating every public header as the same kind of entry point.

They are not the same.

### Top-level runtime facade

```text
src/game/main.h
```

This is the main application boundary the backends talk to.

### Subsystem API

```text
src/game/audio_demo.h
```

That header exposes audio-related functions:

```c
void game_audio_init_demo(GameAudioState *audio);
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf,
                            int num_frames);
void game_audio_update(GameAudioState *audio, float dt_ms);
void game_play_sound_at(GameAudioState *audio, SoundID id);
```

These functions are real and important.

But they are not the top-level runtime facade.

They are a subsystem living inside the larger runtime.

### Safe classification rule

Use this mental model from now on:

```text
src/platform.h        = shared platform/game contract
src/game/main.h       = live runtime front door
src/game/audio_demo.h = subsystem API used by the runtime
```

If you keep this classification straight, later lessons get much easier.

## Step 7: One Complete End-to-End Trace

Take this command:

```bash
./build-dev.sh --backend=raylib
```

Now trace it like a pipeline:

1. The build script chooses the Raylib backend shell.
2. The shared sources still include `src/game/main.c`.
3. Raylib `main.c` initializes shared platform resources.
4. Raylib `main.c` calls `game_app_init(&game, &props.perm)`.
5. Each frame, Raylib gathers input and timing.
6. Raylib calls `game_app_update(...)`.
7. Raylib calls `game_app_render(...)`.
8. The audio path asks the game runtime for samples.

The correct conclusion is not:

```text
Raylib owns the game.
```

The correct conclusion is:

```text
Raylib is one backend shell around the shared game runtime.
```

Now swap `raylib` for `x11` and the same high-level story still holds.

## Visual Summary: Who Owns What?

```text
build-dev.sh
  owns build selection

platform main.c
  owns OS window loop
  owns OS input polling
  owns OS audio device
  owns presentation/upload step

src/platform.h
  owns the shared language between platform and game

src/game/main.h + src/game/main.c
  own the shared runtime behavior

src/game/audio_demo.h + .c
  own one subsystem inside that runtime
```

## Common Beginner Mistakes

### Mistake 1: "If a file is compiled, it must be the front door"

Not true.

Many real codebases compile helper modules, legacy teaching modules, or subsystem implementations that are not the top-level entry point.

### Mistake 2: "Each backend probably has its own game logic"

Not in this course.

The backend shells differ in platform work, but both call the same public game facade.

### Mistake 3: "A public header always means top-level API"

Also false.

Some public headers are subsystem surfaces, not application entry points.

## Worked Analogy for Web Developers

If this were a web app, the structure would feel like this:

```text
build-dev.sh           -> bundler/build config
platform main.c        -> environment adapter
src/platform.h         -> shared host/runtime interface
src/game/main.h/.c     -> app root and core runtime
audio_demo.h/.c        -> one feature module inside the app
```

That analogy is imperfect, but it is good enough to keep your footing.

## Practice

Answer these before moving on.

1. Why is `src/game/main.h` more important than `src/game/audio_demo.h` when you want to understand the top-level runtime flow?
2. Why does it matter that both backends call the same `game_app_*` functions?
3. Why is `src/game/audio_demo.c` still worth studying later even though it is not the main entry point?
4. What mistake are you likely to make if you ignore the comment in `build-dev.sh` about the live runtime routing through `game/main.c`?

## Mini Exercise

Without looking back, redraw this from memory:

```text
build-dev.sh
  -> one backend main.c
  -> src/platform.h
  -> src/game/main.h
  -> src/game/main.c
```

Then add one note explaining where `audio_demo` fits.

If you cannot do that yet, reread Steps 3 through 6.

## Troubleshooting Your Mental Model

### "I still feel like there are too many files"

That is normal.

Reduce the system to roles, not filenames:

```text
build chooser
backend shell
shared contract
game facade
subsystem
```

### "I do not understand why both backends matter yet"

You do not need to understand their internals yet.

For now, you only need to understand why having two shells makes the shared facade important.

### "I keep mixing up live path and teaching-support path"

Return to the build script and the phrase:

```text
the live runtime now routes through game/main.c
```

That sentence is your anchor.

## Recap

You learned five foundational ideas:

1. The active course root is `platform-backend/course/`.
2. `build-dev.sh` chooses one backend shell.
3. Both backend shells call the same public game facade.
4. `src/game/main.h` is the live runtime front door.
5. `audio_demo` is a real subsystem, not the top-level application boundary.

If those five statements feel stable, then Module 01 is already doing its job.

## Next Lesson

Now that you know which runtime is live, the next question becomes:

```text
How does the build script decide what kind of executable we get?
```

Lesson 02 answers that by walking through `build-dev.sh` carefully enough that you can tell the difference between:

- backend choice
- debug vs bench personality
- compile-time behavior switches
- workflow-only flags like `--run`
