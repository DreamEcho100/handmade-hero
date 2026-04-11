# Lesson 30: Arenas, Lifetimes, and `PlatformGameProps`

## Frontmatter

- Module: `07-memory-lifetimes-and-allocators`
- Lesson: `30`
- Status: Required
- Target files:
  - `src/platform.h`
  - `src/game/main.c`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `PlatformGameProps`
  - `platform_game_props_init`
  - `platform_level_arena_reset`
  - `platform_game_props_free`
  - `Arena perm`
  - `Arena level`
  - `Arena scratch`

## Observable Outcome

By the end of this lesson, you should be able to explain why the runtime organizes memory by lifetime instead of by object type, and why `PlatformGameProps` is the right place to own the three main arenas.

## Prerequisite Bridge

Module 06 separated world state, HUD state, and per-frame baked render state.

Module 07 asks the next systems question:

```text
where should all this state live,
and when should it disappear?
```

The course answers that with lifetime buckets rather than a pile of unrelated `malloc(...)` calls.

## Why This Lesson Exists

When beginners first manage memory manually, they often ask:

```text
who frees this object?
```

Arena-based systems push you toward a better question:

```text
what lifetime should this data have?
```

That shift is the foundation of this whole module.

## Step 1: Read `PlatformGameProps` as the Shared Runtime Container

In `src/platform.h`:

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

This struct sits at the platform boundary, so every backend initializes the same storage layout.

That matters because both Raylib and X11 need the same persistent resources:

- a backbuffer
- an audio buffer
- world configuration
- lifetime-partitioned memory

Instead of scattering those allocations across multiple platform files, the runtime centralizes them here.

## Step 2: Name the Three Lifetimes Before Naming Any Objects

The code comments in `src/platform.h` already give the intended meaning:

- `perm`: session lifetime
- `level`: current-scene lifetime
- `scratch`: per-frame lifetime

This is the critical mental model.

Do not start by saying:

```text
enemies go here, strings go there, events go somewhere else
```

Start by saying:

```text
does this survive shutdown only?
does it survive until the next scene rebuild?
does it survive only for this frame?
```

## Step 3: Walk `platform_game_props_init(...)`

The initializer does more than allocate pixels and audio buffers.

It also bootstraps the lifetime model:

```c
if (arena_bootstrap(&props->perm, ARENA_PERM_SIZE, ARENA_PERM_SIZE,
                    ARENA_LIFETIME_SESSION, "perm") != 0)
  ...

if (arena_bootstrap(&props->level, ARENA_LEVEL_SIZE, ARENA_LEVEL_SIZE,
                    ARENA_LIFETIME_LEVEL, "level") != 0)
  ...

if (arena_bootstrap(&props->scratch, ARENA_SCRATCH_SIZE, ARENA_SCRATCH_SIZE,
                    ARENA_LIFETIME_FRAME, "scratch") != 0)
  ...
```

Three important things happen here.

### First

Each arena gets a size budget.

### Second

Each arena gets lifetime metadata.

### Third

Each arena gets a debug name so statistics and failures stay readable.

That is a strong example of the course choosing clarity over cleverness.

## Step 4: Attach Real Runtime Meaning to Each Arena

### `perm`

Session-lifetime storage.

This is for data that should survive until shutdown. If you reset this every scene, you picked the wrong arena.

### `level`

Scene-lifetime storage.

This is the main rebuild bucket. When the scene changes or reloads, this whole arena can be reset and reconstructed.

### `scratch`

Frame-lifetime storage.

This is temporary working memory. The runtime wraps each frame in a `TempMemory` checkpoint so all scratch allocations disappear together.

## Visual: The Lifetime Ladder

```text
perm    : start app ---------------------------------------> shutdown
level   : load scene ------------> reload / scene change
scratch : begin frame -> update/render helpers -> end frame
```

That is the picture you want in your head whenever new data needs a home.

## Step 5: See the Level Reset Boundary in One Line

In `src/platform.h`:

```c
static inline void platform_level_arena_reset(PlatformGameProps *props) {
  arena_reset(&props->level);
}
```

This helper is tiny, but it states the design very clearly:

```text
level data is not individually freed
it is invalidated as one lifetime region
```

That is the core advantage of arenas.

## Step 6: Connect the Level Arena to Actual Scene Rebuilds

In `src/game/main.c`, the rebuild path is:

```c
game->level_state = NULL;
arena_reset(level);
level_state = ARENA_PUSH_ZERO_STRUCT(level, GameLevelState);
```

That is the whole scene-lifetime policy in action.

The runtime does not walk every per-scene allocation and free them one by one.

Instead it says:

```text
the old scene is dead
reset its arena
build the new scene into the same region
```

## Step 7: Connect Scratch Lifetime to the Platform Backends

In both backends, each frame is wrapped like this:

```c
TempMemory frame_scratch = arena_begin_temp(&props.scratch);
game_app_render(..., &props.scratch);
arena_end_temp(frame_scratch);
arena_check(&props.scratch);
```

This means scratch memory is not just "temporary by convention."

It is temporary by enforced lifetime boundary.

If the frame allocates temp strings, temp arrays, or temp HUD data, the backend rewinds all of it at once.

## Step 8: Why `PlatformGameProps` Owns the Arenas

This ownership choice has two benefits.

### Shared platform behavior

Both backends get the same memory layout and cleanup behavior.

### Clear lifetime root

The platform is the thing that creates and destroys the whole runtime session, so it is the right owner for session, scene, and frame arenas.

That keeps memory policy near the platform boundary instead of hiding it deep inside game code.

## Step 9: Read `platform_game_props_free(...)` as Symmetric Shutdown

The cleanup path frees arenas in reverse order:

```c
arena_free(&props->scratch);
arena_free(&props->level);
arena_free(&props->perm);
```

In debug builds it also dumps arena stats first.

That gives the runtime a very clean shutdown story:

- per-frame temp state is gone
- scene state is gone
- session state is gone

All of that happens without keeping a giant list of individual heap allocations.

## Practical Exercises

### Exercise 1: Assign a Lifetime

For each of the following, say which arena it most likely belongs in:

- a string built only while drawing the current frame
- the active scene's entity list
- a long-lived registry that survives scene changes

Expected answer:

```text
frame string -> scratch
scene entities -> level
long-lived registry -> perm
```

### Exercise 2: Explain the Reset Model

Why is `arena_reset(&props->level)` more useful than individually freeing scene objects?

Expected answer:

```text
because the whole scene shares one lifetime, so a single reset is simpler, cheaper, and less error-prone than tracking many individual frees
```

### Exercise 3: Explain Platform Ownership

Why does `PlatformGameProps` own the arenas instead of `GameAppState`?

Expected answer:

```text
because the platform creates and destroys the runtime session and both backends need the same allocation and cleanup policy
```

## Common Mistakes

### Mistake 1: Treating an arena as just a faster heap

The main win is not only speed. It is explicit lifetime grouping.

### Mistake 2: Putting per-frame scratch data into `perm`

That defeats the whole point of rewindable temporary memory.

### Mistake 3: Thinking `level` means one gameplay level only

In this runtime it really means current scene lifetime, including scene reload labs.

## Troubleshooting Your Understanding

### "Why not just keep everything in one big arena?"

Because different data dies at different times. One arena would force you to either over-retain short-lived data or reset long-lived data too aggressively.

### "Why not put scratch inside the game instead of the platform?"

Because every backend frame needs the same scratch boundary, and the platform is what controls the frame loop.

## Recap

You now have the lifetime map for the runtime:

1. `PlatformGameProps` owns the shared memory roots
2. `perm` holds session-lifetime data
3. `level` holds scene-lifetime data and is reset on rebuild
4. `scratch` holds frame-lifetime temp data and is rewound each frame

## Next Lesson

Lesson 31 moves one level deeper and shows how an arena is physically bootstrapped with an `ArenaBlock`, how the initial block is reserved, and how the non-Windows path places guard pages around the usable region.
