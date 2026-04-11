# Lesson 36: `game/main.h`, Scene IDs, and the Narrow Public Surface

## Frontmatter

- Module: `08-game-facade-and-scene-machine`
- Lesson: `36`
- Status: Required
- Target files:
  - `src/game/main.h`
  - `src/game/main.c`
- Target symbols:
  - `GameSceneID`
  - `GAME_SCENE_ARENA_LAB`
  - `GAME_SCENE_LEVEL_RELOAD`
  - `GAME_SCENE_POOL_LAB`
  - `GAME_SCENE_SLAB_AUDIO_LAB`
  - `GAME_SCENE_COUNT`
  - `GameAppState`
  - `game_app_init`
  - `game_app_update`
  - `game_app_render`
  - `game_app_get_audio_samples`
  - `COURSE_FORCE_SCENE_INDEX`
  - `COURSE_LOCK_SCENE`

## Observable Outcome

By the end of this lesson, you should be able to explain why both backends only need one tiny game-facing header, what scene identity data is allowed to cross that boundary, and why the facade stays much smaller than the real runtime behind it.

## Prerequisite Bridge

Modules 03 through 07 taught the major subsystems individually:

- pixels and presentation
- input and frame timing
- audio synthesis and device drain
- coordinate policy and cameras
- allocator lifetimes and scene rebuild ownership

Module 08 asks the next architectural question:

```text
how do all of those systems get driven
without letting the platform backends know every internal detail?
```

The answer is the game facade in `src/game/main.h`.

## Why This Lesson Exists

If Raylib and X11 were allowed to reach directly into every scene struct, the course would lose a very important separation:

```text
platform code should drive the game
without becoming partial game logic itself
```

This lesson defines the narrow surface that preserves that split.

## Step 1: Read the Public Header Before Reading `main.c`

In `src/game/main.h`, the public surface is intentionally small:

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

That is the backend contract.

Nothing else is required for Raylib or X11 to drive the runtime.

## Step 2: Understand What the Header Deliberately Hides

The header does not expose:

- `GameLevelState`
- scene payload unions
- HUD composition helpers
- allocator lab structs
- audio warning logic
- trace panel helpers

That is not accidental omission.

It is the design.

The public boundary gives the backends exactly enough information to:

- create the game
- update the game
- render the game
- request audio samples

and no more.

## Visual: The Boundary

```text
Raylib main / X11 main
        |
        v
   src/game/main.h
        |
        v
 GameAppState + scene machine + HUD + labs + audio + proof panels
```

The backend touches the API surface, not the internal state tree.

## Step 3: Read `GameSceneID` as Stable Scene Vocabulary

The enum is not just for a `switch` statement.

It becomes shared vocabulary across the runtime:

- scene selection
- rebuild logic
- HUD titles and summaries
- audio scene profiles
- exercise tracking arrays

### Active IDs on this branch

```text
0 -> Arena Lifetimes Lab
1 -> Level Reload Lab
2 -> Pool Reuse Lab
3 -> Slab + Audio State Lab
```

### Why `GAME_SCENE_COUNT` matters

It is the array bound that keeps scene-indexed state coherent:

- `exercise[GAME_SCENE_COUNT]`
- `scene_enter_count[GAME_SCENE_COUNT]`
- `audio_warning_heat[GAME_SCENE_COUNT]`

So `GAME_SCENE_COUNT` is not decorative. It defines the valid range for scene-indexed runtime state.

## Step 4: Read the Opaque `GameAppState` as a Boundary Guard

The public header only says:

```c
typedef struct GameAppState GameAppState;
```

That means backends can hold a pointer, pass it back into facade functions, and nothing more.

They cannot legally do this:

```text
game->audio...
game->level_state...
game->scene...
```

because the full struct layout is hidden.

That is the point.

## Worked Example: Why Opacity Matters

Suppose `main.h` exposed the full layout of `GameAppState`.

Very quickly, platform code could start doing things like:

- forcing scene changes directly
- reading HUD log state directly
- mutating audio or override fields directly

Then the backend would stop being only a platform shell and start becoming part of the game runtime.

The forward declaration prevents that drift.

## Step 5: Map the Four Public Functions to Runtime Responsibilities

### `game_app_init(...)`

Creates session-lifetime game state.

### `game_app_update(...)`

Advances scene policy, scene simulation, exercise state, warning state, and frame-level audio updates.

### `game_app_render(...)`

Builds render contexts, draws the scene, and draws the proof-oriented HUD.

### `game_app_get_audio_samples(...)`

Fills the shared `AudioOutputBuffer` when the backend's drain loop requests more sound.

That split mirrors the course's multi-system architecture very cleanly.

## Step 6: Notice the Lifetime Hints Inside the Signatures

The function signatures already expose a lot of design information.

### `game_app_init(GameAppState **out_game, Arena *perm)`

This says the root game state belongs in the session-lifetime arena.

### `game_app_update(..., Arena *level)`

This says update owns the scene-lifetime rebuild boundary.

### `game_app_render(..., Arena *perm, Arena *level, Arena *scratch)`

This says rendering can inspect long-lived and scene-owned state while using scratch for frame-local temporary work.

So even before opening `main.c`, the API is already advertising the allocator model from Module 07.

## Step 7: Tie the Header Back to `game_app_init(...)`

In `src/game/main.c`, init begins like this:

```c
game = ARENA_PUSH_ZERO_STRUCT(perm, GameAppState);
```

That is the concrete proof of the signature's meaning.

The root app state is not heap-scattered or platform-owned. It is born directly in `perm`.

## Step 8: Notice the Compile-Time Scene Override Hooks

At the top of `src/game/main.c`, the branch defines:

```c
#ifndef COURSE_FORCE_SCENE_INDEX
#define COURSE_FORCE_SCENE_INDEX (-1)
#endif

#ifndef COURSE_LOCK_SCENE
#define COURSE_LOCK_SCENE 0
#endif
```

And inside `game_app_init(...)`:

```c
if (COURSE_FORCE_SCENE_INDEX >= 0 &&
    COURSE_FORCE_SCENE_INDEX < GAME_SCENE_COUNT) {
  start_scene = (GameSceneID)COURSE_FORCE_SCENE_INDEX;
  game->scene.compile_time_override_active = 1;
  game->scene.compile_time_locked = COURSE_LOCK_SCENE ? 1 : 0;
  game->scene.compile_time_scene = start_scene;
}
```

This is important for two reasons.

### First

Scene IDs are stable enough to be used by build-time forcing.

### Second

The backend still does not need to understand the scene machine to benefit from that feature. It just calls `game_app_init(...)` and the game layer applies the override internally.

## Step 9: The Header Tells the Backends What They Are Allowed to Know

The backends are allowed to know:

- that scenes have IDs
- that game state is an opaque handle
- that the game exposes init/update/render/audio entry points

They are not allowed to know:

- which scene owns which pool/slab struct
- how proof panels are assembled
- how override policy is resolved internally

That is exactly the right constraint for this course.

## Practical Exercises

### Exercise 1: Explain Opaque State

Why is `GameAppState` forward-declared in the header instead of fully defined there?

Expected answer:

```text
so the backends can hold and pass the game handle without depending on internal runtime layout or mutating private fields directly
```

### Exercise 2: Explain `GAME_SCENE_COUNT`

Why is `GAME_SCENE_COUNT` more important than just “one more enum value”?

Expected answer:

```text
because it defines the valid scene range and sizes multiple scene-indexed arrays across the runtime
```

### Exercise 3: Explain the API Split

Why does the facade expose separate update, render, and audio entry points?

Expected answer:

```text
because simulation, drawing, and backend-driven audio chunk filling are different runtime responsibilities and should remain separated
```

## Common Mistakes

### Mistake 1: Thinking a facade is just boilerplate

It is the enforcement point that keeps the platform shell from swallowing game internals.

### Mistake 2: Treating scene IDs as only UI labels

They are also stable keys for rebuild logic, audio profile switching, and proof tracking arrays.

### Mistake 3: Assuming the backends should know scene structs directly

That would break the point of the facade.

## Troubleshooting Your Understanding

### "Why not let the backend call scene helpers directly if it's simpler?"

Because short-term convenience would couple the backend to private runtime layout and make every later refactor more painful.

### "Why is `game_app_init(...)` the right place for compile-time scene forcing?"

Because initialization is the first point where the game layer can choose a valid starting scene without exposing scene-machine internals through the public API.

## Recap

You now understand the public game boundary:

1. `src/game/main.h` is intentionally tiny
2. `GameSceneID` is stable shared scene vocabulary
3. `GameAppState` is opaque to protect internal structure
4. the four facade functions are enough for both backends to drive the runtime
5. compile-time scene forcing already fits inside this boundary without widening it

## Next Lesson

Lesson 37 opens `src/game/main.c` and maps the ownership tree behind the opaque handle: `GameAppState`, `GameSceneMachine`, `GameLevelState`, and the split between session-lifetime control state and rebuildable scene payload.
