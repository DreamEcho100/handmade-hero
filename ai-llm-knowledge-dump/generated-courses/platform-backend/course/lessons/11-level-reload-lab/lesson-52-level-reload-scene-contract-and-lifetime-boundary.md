# Lesson 52: Level Reload Scene Contract and Lifetime Boundary

## Frontmatter

- Module: `11-level-reload-lab`
- Lesson: `52`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/game/main.h`
- Target symbols:
  - `GAME_SCENE_LEVEL_RELOAD`
  - `LevelReloadScene`
  - `LevelFormation`
  - `LevelEntity`
  - `GameLevelState`
  - `game_scene_name`
  - `game_scene_summary`
  - `game_scene_action_hint`

## Observable Outcome

By the end of this lesson, you should be able to explain the Level Reload Lab as one rebuild-focused scene contract: what payload it owns, what lifetime boundary it teaches, and why `R` and `SPACE` are intentionally contrasted.

## Why This Lesson Exists

Lesson 51 finished the Arena module with one clear pattern:

```text
pick one scene
lock the learner into it
prove one allocator story end to end
```

The next allocator story cannot just be another nested `TempMemory` example.

It needs a different ownership boundary.

The Level Reload Lab is that boundary.

Here the important question is not:

```text
what transient branch gets rewound?
```

It is:

```text
what whole chunk of scene-owned data gets torn down and rebuilt together?
```

## First Reading Strategy

Read this lab from contract to payload shape:

1. scene labels and learner-facing promise
2. `LevelReloadScene`
3. `LevelFormation` versus `LevelEntity`
4. how the scene fits inside `GameLevelState`

That order keeps the ownership boundary clear before you look at the rebuild mechanism.

## Step 0: Compare This Lab To The Arena Lab First

Before reading the scene structs, ask:

```text
what changed between the last lab and this one?
```

The answer is not only the visuals. The ownership question changed from nested transient checkpoints to one grouped scene payload that can be regenerated together.

## Step 1: Read the Scene Labels as the Contract

The scene first appears through three strings in `src/game/main.c`:

- `game_scene_name(...)` -> `Level Reload Lab`
- `game_scene_summary(...)` -> `scene-local data gets rebuilt on reload`
- `game_scene_action_hint(...)` -> `SPACE spike the cache scan and audio warning line`

These three strings already define the lesson.

The summary says the ownership rule.

The action hint says the contrast you are supposed to observe.

`R` rebuilds the scene.

`SPACE` only stresses the scan and audio layer.

That difference is the core of this module.

## Step 2: Read `LevelReloadScene` as Rebuildable Level-Owned State

The scene payload is:

```c
typedef struct {
  GameCamera camera;
  LevelFormation *formations;
  int formation_count;
  LevelEntity *entities;
  int entity_count;
  unsigned int build_serial;
  unsigned int reload_seed;
  float pulse;
  float rebuild_flash;
  float scan_t;
  int highlighted_entity;
} LevelReloadScene;
```

This struct is much simpler than the Arena lab.

That is deliberate.

The scene only needs enough state to prove five things:

- there is scene-owned data
- it gets allocated together
- reloads replace it with fresh data
- scans can traverse it without rebuilding it
- visual or audio pressure does not automatically mean allocator work

## Visual: What This Scene Is Actually Holding

```text
LevelReloadScene
  -> cluster headers      (formations)
  -> entity payload       (entities)
  -> rebuild evidence     (build_serial, reload_seed, rebuild_flash)
  -> read-only traversal  (scan_t, highlighted_entity)
  -> view state           (camera, pulse)
```

The lab is not about complex behavior.

It is about clean ownership boundaries.

## Step 3: Separate `LevelFormation` From `LevelEntity`

Two structs matter here.

### `LevelFormation`

Each formation is a cluster header.

It stores:

- cluster position and size
- cluster color
- `entity_start`
- `entity_count`
- a human label such as `cluster_1`

### `LevelEntity`

Each entity is one actual payload record.

It stores:

- position and size
- current `heat`
- color
- `formation_index`
- `serial`

This distinction is important.

The formations organize the scene.

The entities are the actual contiguous strip the scene wants to prove.

## Step 4: Notice Where This Scene Lives Inside `GameLevelState`

`GameLevelState` owns one active scene payload at a time:

```c
typedef struct {
  GameSceneID scene_id;
  const char *scene_name;
  union {
    ArenaLabScene arena_lab;
    LevelReloadScene level_reload;
    PoolLabScene pool_lab;
    SlabAudioScene slab_audio_lab;
  } as;
} GameLevelState;
```

That means the Level Reload Lab is not global app state.

It is one rebuildable scene payload inside the level arena.

So the lifetime rule is:

```text
scene switch or manual reload
  -> rebuild GameLevelState
  -> replace LevelReloadScene with a fresh copy
```

## Step 5: Read `build_serial` and `reload_seed` as Proof Hooks

Two fields matter immediately:

- `build_serial`
- `reload_seed`

They exist for proof.

`build_serial` tells you which rebuild produced the current scene.

`reload_seed` tells you why the data layout changed.

Without those fields, a learner could see the screen change and still wonder whether the runtime really rebuilt anything or only replayed an animation.

With them, the scene can say:

```text
this is rebuild number N
and it produced seeded layout N
```

## Worked Example: Why `SPACE` Belongs In The Scene Contract

The action hint mentions `SPACE` because the lab is not only about showing rebuild. It is about showing a second event that looks active without actually reallocating the payload.

That contrast is why the scene contract needs both `R` and `SPACE` in the learner's mental model from the start.

## Step 6: Notice the Pedagogical Shift From Arena to Level Reload

The Arena lab taught:

- nesting
- rewind
- return to baseline

The Level Reload lab teaches:

- teardown
- rebuild
- stable traversal after rebuild
- transient scan pressure without ownership change

So the extension is widening the learner's allocator vocabulary.

It is no longer only about temp scopes.

It is about scene-local payload ownership.

## Common Mistake: Thinking This Scene Is About Streaming or Save Data

It is not.

The scene is not teaching persistent world serialization.

It is teaching a smaller and more important rule first:

```text
when one scene owns one block of level data,
the cleanest reset is often to rebuild that whole block together
```

## JS-to-C Bridge

This is like rebuilding one route-local view model and its cached rows all at once instead of mutating dozens of objects individually. The point is not that the data changes. The point is that ownership is clear enough that reset can be total and cheap.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find the three Level Reload strings and explain what they imply about `R` versus `SPACE`.
2. Read `LevelReloadScene` and identify which fields prove rebuilds and which only drive traversal or presentation.
3. Read `LevelFormation` and `LevelEntity` and explain why the scene needs both.
4. Find the `GameLevelState` union and explain what it means for the Level Reload scene's lifetime.

## Exercises

1. Explain why the Level Reload Lab is a different allocator lesson from the Arena Lab.
2. Explain why `build_serial` and `reload_seed` belong in the scene struct instead of only in global counters.
3. Explain why `formations` and `entities` should be mentally treated as headers versus payload.
4. Describe the lifetime boundary of `LevelReloadScene` in one sentence.

## Runtime Proof Checkpoint

At this point, you should be able to explain the scene contract: the Level Reload Lab visualizes one block of scene-owned level data that is rebuilt on reload, then scanned and stressed without changing ownership until the next rebuild.

## Recap

This lesson established the Level Reload Lab teaching surface:

- the scene labels tell you reload is the main ownership boundary
- `LevelReloadScene` stores rebuild evidence plus scan state
- formations organize the scene while entities form the real payload
- the whole payload lives inside rebuildable `GameLevelState`

Next, we trace the exact rebuild path so the learner can see what survives a reload, what gets reset, and why the level arena is the right owner for this scene.
