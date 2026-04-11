# Lesson 37: `GameAppState`, `GameLevelState`, and `GameSceneMachine`

## Frontmatter

- Module: `08-game-facade-and-scene-machine`
- Lesson: `37`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/game/main.h`
  - `src/utils/audio.h`
- Target symbols:
  - `GameLevelState`
  - `GameSceneMachine`
  - `GameAppState`
  - `GameSceneExerciseState`
  - `GameHudLogEntry`
  - `scene`
  - `audio`
  - `level_state`

## Observable Outcome

By the end of this lesson, you should be able to draw the ownership tree behind the opaque `GameAppState *` handle and explain which parts survive scene rebuilds, which parts are scene-local, and which parts are pure control metadata rather than payload.

## Prerequisite Bridge

Lesson 36 explained the outside of the game boundary.

Now we step behind that boundary and answer the next question:

```text
what actually lives inside the game layer,
and how is it divided by lifetime and responsibility?
```

This lesson is where Module 07's arena model becomes visibly useful.

## Why This Lesson Exists

If you only look at the update and render functions as control flow, `main.c` can feel like a large file full of branching.

It becomes much clearer once you split the state into three layers:

```text
session-lifetime root state
scene-machine control state
current-scene payload
```

That is what this lesson makes explicit.

## Step 1: Start With the Smallest Proof State Near the Top

At the beginning of `src/game/main.c`, one early struct is:

```c
typedef struct {
  unsigned int progress_mask;
  unsigned long long completed_frame;
} GameSceneExerciseState;
```

That is a useful reminder that `GameAppState` is not only simulation data.

It also carries proof/instrumentation state that survives scene switches.

So even before reading the big structs, you should expect the root app state to contain more than just "current gameplay objects."

## Step 2: Read `GameLevelState` as the Rebuildable Scene Payload Shell

The scene-lifetime struct is:

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

This is the active-scene payload container.

It stores:

- which scene the payload belongs to
- a readable scene name
- one active scene payload in the union

## Why the Union Is Correct Here

Only one scene is active at a time.

So the runtime does not need four simultaneous payload trees inside `level_state`.

The union expresses the real rule:

```text
only one scene-local payload is valid right now
```

That is both a memory optimization and a structural statement.

## Visual: Scene Payload Ownership

```text
GameLevelState
  -> scene_id / scene_name
  -> one active payload in the union
     arena_lab OR level_reload OR pool_lab OR slab_audio_lab
```

This is exactly what you want for level-arena-backed state.

## Step 3: Read `GameSceneMachine` as Policy State, Not Content

The control struct is:

```c
typedef struct {
  GameSceneID current;
  GameSceneID requested;
  GameSceneID previous;
  GameSceneID compile_time_scene;
  GameSceneID runtime_forced_scene;
  unsigned long long frame_entered;
  int compile_time_override_active;
  int compile_time_locked;
  int runtime_override_active;
  int runtime_override_locked;
} GameSceneMachine;
```

This struct does not own scene payload data.

It owns scene-selection policy and transition bookkeeping.

That distinction is extremely important.

### `GameSceneMachine` answers:

- what scene is active now?
- what scene is requested next?
- what scene was active before?
- are overrides currently forcing a different answer?

### `GameLevelState` answers:

- what data does the active scene own right now?

Those are different questions, so they live in different structs.

## Step 4: Read the Root `GameAppState`

The session-lifetime root is:

```c
struct GameAppState {
  GameSceneMachine scene;
  GameAudioState audio;
  GameLevelState *level_state;
  GameSceneID rebuild_failure_scene;
  GameSceneExerciseState exercise[GAME_SCENE_COUNT];
  int hud_visible;
  int rebuild_failure_active;
  unsigned long long frame_index;
  unsigned long long rebuild_failure_frame;
  unsigned int scene_enter_count[GAME_SCENE_COUNT];
  unsigned int scene_reload_count[GAME_SCENE_COUNT];
  unsigned int transition_count;
  unsigned int rebuild_failure_count;
  GameHudLogEntry logs[GAME_ARENA_LOG_CAPACITY];
  int log_head;
  int log_count;
  float audio_warning_heat[GAME_SCENE_COUNT];
  float audio_warning_peak[GAME_SCENE_COUNT];
};
```

This is the ownership tree root that lives in the `perm` arena.

## Step 5: Group the Root Fields by Responsibility

The struct looks less intimidating if you sort it conceptually.

### Scene-control metadata

- `scene`
- `transition_count`
- `frame_index`

### Cross-scene audio state

- `audio`

### Current scene payload pointer

- `level_state`

### Failure and lifecycle tracking

- `rebuild_failure_scene`
- `rebuild_failure_active`
- `rebuild_failure_frame`
- `rebuild_failure_count`
- `scene_enter_count[]`
- `scene_reload_count[]`

### Proof and HUD instrumentation

- `exercise[]`
- `logs[]`
- `log_head`
- `log_count`
- `audio_warning_heat[]`
- `audio_warning_peak[]`
- `hud_visible`

That grouping is the real shape of the runtime.

## Visual: Ownership Tree

```text
GameAppState (perm / session lifetime)
├── scene                -> policy and transition metadata
├── audio                -> long-lived audio subsystem state
├── level_state*         -> current scene payload in level arena
├── exercise[]           -> persistent proof progress per scene
├── logs[]               -> HUD/event ring buffer
├── enter/reload counts  -> cross-scene lifecycle evidence
└── warning heat/peak    -> per-scene audio diagnostic memory

GameLevelState (level / scene lifetime)
├── scene_id
├── scene_name
└── active scene payload union
```

This is the picture you should keep in mind for the whole rest of Module 08.

## Step 6: Tie the Structs Back to Arena Lifetimes

Now map them onto Module 07's memory buckets.

### `perm`

- `GameAppState`
- `GameSceneMachine`
- `GameAudioState`
- exercise state
- logs and counters

### `level`

- the current `GameLevelState`
- whatever arrays, pools, or slabs that scene payload allocates into the level arena

### `scratch`

- render-time temp strings and nested temp scopes during a frame

This is not just an implementation detail. It is how the runtime explains what survives scene rebuild versus what dies with the scene.

## Worked Example: Why `level_state` Is a Pointer

`GameAppState` does not store `GameLevelState` inline.

Instead it stores:

```c
GameLevelState *level_state;
```

That is the correct design because the scene payload belongs to the `level` arena, not to the `perm`-resident app root.

If `level_state` were inline inside `GameAppState`, the scene payload would stop participating in grouped scene teardown naturally.

The pointer keeps the lifetime boundary honest.

## Step 7: Notice How Scene Payload Types Already Carry Scene-Owned Cameras

Each scene payload struct includes a `GameCamera` field.

For example:

- `ArenaLabScene.camera`
- `LevelReloadScene.camera`
- `PoolLabScene.camera`
- `SlabAudioScene.camera`

That means camera ownership belongs to the scene payload, not to the scene machine and not to the backend.

This becomes important when Lesson 40 talks about scene-enter side effects.

## Step 8: Notice That Proof State and Logs Live Outside `level_state`

The trace system is intentionally not scene-local only.

Arrays such as:

- `exercise[GAME_SCENE_COUNT]`
- `scene_enter_count[GAME_SCENE_COUNT]`
- `scene_reload_count[GAME_SCENE_COUNT]`
- `audio_warning_heat[GAME_SCENE_COUNT]`

live in the session root because the runtime wants to remember proof and lifecycle information across scene transitions.

That is a subtle but important design choice.

If these fields lived inside the level arena, a scene rebuild would erase the very evidence the course is trying to teach from.

## Step 9: Read `GameHudLogEntry` as Persistent Teaching Memory

The log entry type is small:

```c
typedef struct {
  char text[128];
} GameHudLogEntry;
```

But conceptually it matters a lot.

The runtime keeps a ring buffer of important events in session-lifetime memory so the HUD can show recent proof and transition history even after the scene payload itself has changed.

Again, that is a strong sign that `GameAppState` is more than simulation state. It is also teaching-state infrastructure.

## Practical Exercises

### Exercise 1: Explain the Split

Why is `GameSceneMachine` not the same thing as `GameLevelState`?

Expected answer:

```text
because the scene machine stores scene-selection policy and transition metadata, while GameLevelState stores the active scene's actual payload data
```

### Exercise 2: Explain `level_state`

Why is `level_state` a pointer in `GameAppState` instead of inline scene payload storage?

Expected answer:

```text
because the active scene payload belongs to the level arena and must be replaceable as one grouped scene-lifetime allocation region
```

### Exercise 3: Explain Persistent Proof State

Why do arrays like `exercise[]` and `scene_enter_count[]` belong in `GameAppState` rather than in `GameLevelState`?

Expected answer:

```text
because the runtime wants that evidence to survive scene rebuilds and transitions instead of being erased with the current scene payload
```

## Common Mistakes

### Mistake 1: Thinking `level_state` is the whole game

It is only the current scene-owned payload.

### Mistake 2: Thinking the scene machine stores gameplay content

It stores policy and transition metadata, not scene-local arrays, pools, or slabs.

### Mistake 3: Assuming every field in `GameAppState` exists for gameplay only

Many fields exist to support HUD proof, warnings, and lifecycle evidence.

## Troubleshooting Your Understanding

### "Why doesn't the runtime put everything into one giant root struct?"

Because different data has different lifetimes. The root struct keeps long-lived control and evidence state; the current scene payload stays rebuildable in the level arena.

### "Why keep per-scene arrays if only one scene is active at a time?"

Because the runtime wants to retain historical or cross-scene evidence, not only current-scene payload.

## Recap

You now understand the internal ownership tree:

1. `GameAppState` is the session-lifetime root in `perm`
2. `GameSceneMachine` stores control policy and transition metadata
3. `GameLevelState` stores the active scene-owned payload in `level`
4. persistent proof and warning state deliberately survive scene rebuilds

## Next Lesson

Lesson 38 follows the first important control path through this state tree: how raw input becomes a scene request, how requests wrap around valid scene IDs, and how compile-time and runtime override rules choose the final authoritative `requested` scene.
