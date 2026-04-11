# Lesson 39: Scene Rebuild Flow and Level-Arena Teardown

## Frontmatter

- Module: `08-game-facade-and-scene-machine`
- Lesson: `39`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/platform.h`
- Target symbols:
  - `game_app_update`
  - `game_rebuild_scene`
  - `level_state`
  - `scene_reload_count`
  - `scene_enter_count`
  - `rebuild_failure_active`
  - `rebuild_failure_scene`
  - `rebuild_failure_count`
  - `platform_level_arena_reset`
  - `arena_reset`

## Observable Outcome

By the end of this lesson, you should be able to explain exactly when the runtime decides a rebuild is necessary, what data dies at the level-arena boundary, what survives in `perm`, and how the new current scene payload is reconstructed afterward.

## Prerequisite Bridge

Lesson 38 ended with one authoritative `requested` scene value.

That still is not a scene transition yet.

This lesson covers the next step:

```text
when does the runtime actually rebuild scene-owned state,
and what does that rebuild destroy?
```

This is where Module 07's lifetime model pays off most directly.

## Why This Lesson Exists

Without grouped scene teardown, every scene change would require many tiny destruction paths.

This branch avoids that by making the level arena the official boundary for scene-local ownership.

So the important question is not only "does the scene ID change?"

It is:

```text
what lifetime bucket gets reset,
and what gets rebuilt into it?
```

## Step 1: Start in `game_app_update(...)`, Not `game_rebuild_scene(...)`

The transition decision lives in the update loop:

```c
if (!game->level_state) {
  game_rebuild_scene(game, level, 1);
} else if (game->scene.requested != game->scene.current) {
  game->scene.previous = game->scene.current;
  game->scene.current = game->scene.requested;
  game->scene.frame_entered = game->frame_index;
  game->transition_count++;
  game_rebuild_scene(game, level, 1);
} else if (do_reload) {
  game_rebuild_scene(game, level, 0);
}
```

This block expresses three distinct rebuild cases.

## Step 2: Separate the Three Cases Explicitly

### Case A: First build

```text
no level_state exists yet
```

The runtime has not constructed any scene payload yet, so it builds one.

### Case B: Transition to another scene

```text
requested != current
```

The runtime records `previous`, promotes `requested` into `current`, timestamps the scene enter, increments transition count, and rebuilds.

### Case C: Reload the same scene

```text
do_reload is true
```

The runtime keeps the same scene identity but rebuilds the current scene payload from scratch.

These are not the same event semantically, even though they all invoke the same rebuild function.

## Visual: Rebuild Decision Tree

```text
no level_state?
  -> first build

else requested != current?
  -> transition + rebuild

else reload pressed?
  -> same-scene rebuild
```

That is the whole entry point into scene reconstruction.

## Step 3: Read `game_rebuild_scene(...)` as the Scene-Lifetime Reset Boundary

The rebuild function begins with:

```c
current = game->scene.current;
game->level_state = NULL;
arena_reset(level);
level_state = ARENA_PUSH_ZERO_STRUCT(level, GameLevelState);
```

This is the architectural core of the lesson.

### `game->level_state = NULL`

Temporarily clears the current-scene pointer so the runtime does not pretend the old scene payload is still valid.

### `arena_reset(level)`

Destroys all level-arena allocations from the previous scene lifetime.

### `ARENA_PUSH_ZERO_STRUCT(level, GameLevelState)`

Begins the new scene lifetime by allocating a fresh root payload object inside the same level arena.

## Worked Example: What `arena_reset(level)` Conceptually Destroys

The level arena may currently contain:

- the previous `GameLevelState`
- level-reload formations and entities
- pool-lab pool backing storage
- slab-audio slab pages and events

After `arena_reset(level)`, all of that scene-lifetime content is gone as one grouped ownership event.

What survives?

- `GameAppState`
- scene-machine control fields
- audio state
- logs and proof masks
- counters and warning history

That survival split is exactly why the runtime uses separate arenas.

## Step 4: The Failure Path Is Part of the Design, Not an Afterthought

If the new `GameLevelState` allocation fails, the runtime records that explicitly:

```c
game->rebuild_failure_active = 1;
game->rebuild_failure_scene = current;
game->rebuild_failure_frame = game->frame_index;
game->rebuild_failure_count++;
game_log_eventf(game, "scene rebuild failed: %s", game_scene_name(current));
```

This is important because rebuild is a true failure boundary.

The runtime does not silently continue as if a current scene payload exists when it does not.

## Step 5: Read the Scene-Specific Fill Switch as "Repopulate the Fresh Level Lifetime"

After allocation, the function does:

```c
level_state->scene_id = current;
level_state->scene_name = game_scene_name(current);
switch (current) {
case GAME_SCENE_ARENA_LAB:
  game_fill_arena_lab(level_state, level, game->scene_reload_count[current]);
  break;
case GAME_SCENE_LEVEL_RELOAD:
  game_fill_level_reload(level_state, level,
                         game->scene_reload_count[current]);
  break;
case GAME_SCENE_POOL_LAB:
  game_fill_pool_lab(level_state, level);
  break;
case GAME_SCENE_SLAB_AUDIO_LAB:
  game_fill_slab_audio(level_state, level);
  break;
}
```

That is the refill half of the reset-and-rebuild story.

The scene machine chooses the scene ID.

The fill function populates the current scene's ownership tree inside the fresh level arena.

## Step 6: Understand `entering_new_scene` as a Semantic Flag

The function increments:

```c
game->scene_reload_count[current]++;
if (entering_new_scene)
  game->scene_enter_count[current]++;
```

This preserves a very useful distinction.

### Reload count

How many times this scene payload has been rebuilt.

### Enter count

How many times the runtime actually entered this scene as a transition event.

These counts should diverge in the reload case, and that divergence is intentional.

## Worked Example: Same-Scene Reload

Suppose the learner stays in `GAME_SCENE_LEVEL_RELOAD` and presses `R` three times.

Then:

- `scene_reload_count[level_reload]` increases on every rebuild
- `scene_enter_count[level_reload]` does not increase for those same-scene reloads

That is the exact semantic distinction the code is preserving.

## Step 7: Connect `platform_level_arena_reset(...)`

In `src/platform.h`, the shared helper is:

```c
static inline void platform_level_arena_reset(PlatformGameProps *props) {
  arena_reset(&props->level);
}
```

Even though `game_rebuild_scene(...)` currently calls `arena_reset(level)` directly, the helper still makes the architecture explicit:

```text
scene teardown means resetting the level arena
```

That is the intended contract between platform-owned memory buckets and game-owned scene rebuilding.

## Step 8: Why `current = requested` Happens Before Rebuild

In the transition case, the runtime promotes `requested` into `current` before calling `game_rebuild_scene(...)`.

That is correct because the rebuild function needs to know which scene payload to construct.

So the order is:

```text
policy chooses requested
update promotes requested into current
rebuild constructs current scene payload
```

This is a clean separation of responsibilities.

## Step 9: The Level Arena Is the Real Teardown Mechanism

The lesson becomes much simpler once you accept this rule:

```text
scene teardown is not an object-by-object free pass
it is a lifetime reset
```

That is the root difference between this runtime and a more ad-hoc ownership model.

## Practical Exercises

### Exercise 1: Explain the Three Cases

What three situations in `game_app_update(...)` can call `game_rebuild_scene(...)`?

Expected answer:

```text
first build when no level_state exists, transition when requested differs from current, and same-scene rebuild when reload is requested
```

### Exercise 2: Explain `arena_reset(level)`

What does `arena_reset(level)` destroy conceptually?

Expected answer:

```text
all current scene-lifetime allocations in the level arena, including the previous GameLevelState and all scene-owned data allocated beneath it
```

### Exercise 3: Explain Enter vs Reload Counts

Why does the function increment reload count on every rebuild but enter count only when `entering_new_scene` is true?

Expected answer:

```text
because reloading the current scene rebuilds ownership without representing a new scene transition, so the runtime tracks those as different kinds of events
```

## Common Mistakes

### Mistake 1: Thinking the scene switch is only `current = requested`

That assignment only picks identity. The real work is the grouped teardown and rebuild that follows.

### Mistake 2: Thinking reload and transition are the same

They share a rebuild function, but they mean different lifecycle events.

### Mistake 3: Forgetting that proof and audio state survive rebuild

Those live in `perm`, not in the level arena.

## Troubleshooting Your Understanding

### "Why clear `game->level_state` before allocating the new one?"

So the runtime never carries a stale pointer to scene data that is about to be invalidated or has just been invalidated.

### "Why not call each scene's destructor instead of resetting the arena?"

Because the scene lifetime is intentionally grouped, and the arena reset is the grouped destruction mechanism.

## Recap

You now understand the rebuild boundary:

1. `game_app_update(...)` decides whether a rebuild is needed
2. transitions, first-build, and reload are distinct cases
3. `game_rebuild_scene(...)` clears the scene pointer and resets the level arena
4. a fresh `GameLevelState` is allocated and repopulated for the current scene
5. reload and enter counts preserve the semantic difference between rebuild types

## Next Lesson

Lesson 40 closes Module 08 by following the cross-system side effects that happen after or around scene enter: default camera ownership, scene-specific camera access, audio profile switching, entry cues, and transition timestamps.
