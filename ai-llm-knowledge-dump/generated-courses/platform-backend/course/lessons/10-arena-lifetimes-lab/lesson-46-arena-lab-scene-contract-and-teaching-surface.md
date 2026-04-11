# Lesson 46: Arena Lab Scene Contract and Teaching Surface

## Frontmatter

- Module: `10-arena-lifetimes-lab`
- Lesson: `46`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/game/main.h`
- Target symbols:
  - `GAME_SCENE_ARENA_LAB`
  - `ArenaLabScene`
  - `ArenaLabBar`
  - `ArenaCheckpointViz`
  - `game_scene_name`
  - `game_scene_summary`
  - `game_scene_action_hint`
  - `game_fill_arena_lab`

## Observable Outcome

By the end of this lesson, you should be able to explain the Arena Lab as one coherent scene contract: what it promises to teach, what state it owns, and why its teaching surface is structured the way it is.

## Why This Lesson Exists

Lesson 45 ended with one big promise:

```text
the runtime overlay is trustworthy because it is built from live state
```

That is the right foundation.

But the next thing the learner actually needs is not more generic HUD theory.

They need one full lab they can trace from scene data, to scene behavior, to scene proof.

The Arena Lifetimes Lab is the right starting point because it teaches the whole lifetime vocabulary in one contained simulation:

- `perm`
- `level`
- `scratch`
- nested `TempMemory`
- rewind
- return to baseline

## First Reading Strategy

Read this lab from contract to payload:

1. scene ID and learner-facing labels
2. the big scene struct
3. the smaller helper structs that define the visual language
4. the fill function that boots the lab into a known starting state

That order keeps the lab readable as a teaching surface instead of a random pile of scene fields.

## Step 0: Ask Why This Is The First Lab

Before reading the scene data, ask one question:

```text
why does the course begin the lab track with arenas instead of reloads, pools, or slabs?
```

The answer is that this scene shows the cleanest visible before-and-after lifetime story. That is the framing to keep in mind while reading every field.

## Step 1: Read the Scene ID and Human Labels Together

In `src/game/main.h` and `src/game/main.c`, the Arena lab appears first as:

```c
GAME_SCENE_ARENA_LAB
```

and then as three human-facing strings:

- `game_scene_name(...)` -> `Arena Lifetimes Lab`
- `game_scene_summary(...)` -> `perm vs level vs scratch vs TempMemory`
- `game_scene_action_hint(...)` -> `SPACE advance checkpoint stack and stack cue pressure`

That is not only presentation.

Those strings define the intended teaching contract of the scene.

The summary tells you what concept is under test.

The action hint tells you what learner input should stress it.

## Step 2: Read `ArenaLabScene` as the Whole Live Simulation State

The scene struct is:

```c
typedef struct {
  GameCamera camera;
  ArenaLabBar *bars;
  int bar_count;
  unsigned int build_serial;
  ArenaCheckpointViz checkpoints[GAME_ARENA_CHECKPOINT_CAPACITY];
  int checkpoint_count;
  unsigned int phase_index;
  unsigned int cycle_count;
  unsigned int rewind_count;
  unsigned int manual_steps;
  int active_depth;
  float phase_timer;
  float beat;
  size_t simulated_temp_bytes;
  size_t peak_temp_bytes;
  const char *phase_name;
  const char *phase_summary;
} ArenaLabScene;
```

This is a very dense teaching surface.

It mixes four kinds of state:

- render-facing scene props such as bars and checkpoints
- simulation progress such as `phase_index` and `phase_timer`
- proof metrics such as `manual_steps`, `rewind_count`, `active_depth`
- human-facing teaching strings such as `phase_name` and `phase_summary`

That means this scene is not a toy in the narrow sense.

It is a purpose-built runtime demonstrator.

## Worked Example: Why The Scene Stores Explanatory Strings Directly

`phase_name` and `phase_summary` live inside `ArenaLabScene` because the lab's current explanation is part of the scene state, not just a generic HUD concern.

That means when the scene changes phase, the explanation changes with the scene itself. The HUD is only rendering that meaning. It is not inventing it later.

## Visual: What the Scene Struct Is Really Holding

```text
ArenaLabScene
  -> what to draw        (bars, checkpoint viz)
  -> where the scene is  (phase index, timer, beat)
  -> what has been proved (manual steps, rewinds, depth, peak temp)
  -> how to explain it   (phase name, phase summary)
```

That is why the lab can teach and not only animate.

## Step 3: Separate `ArenaLabBar` From `ArenaCheckpointViz`

Two smaller structs define the lab's visual language.

### `ArenaLabBar`

Each bar holds:

- title
- summary
- world-space rect
- color

These are the four persistent lifetime buckets shown in the scene: `perm`, `level`, `scratch`, and `temp`.

### `ArenaCheckpointViz`

Each checkpoint holds:

- label
- `used_bytes`
- fill amount
- color

These are not the lifetime buckets themselves.

They are the live nested temp checkpoints inside the current phase.

That distinction matters.

```text
bars        -> the big allocator vocabulary
checkpoints -> the current transient checkpoint stack
```

## Step 4: Read `game_fill_arena_lab(...)` as Scene Boot, Not Simulation Advance

The fill function does three major things:

1. install default scene-owned camera state
2. allocate and populate the four persistent bars in the level arena
3. reset the phase/proof counters and apply the starting phase

The core setup is:

```c
scene->camera = game_default_camera();
scene->bar_count = 4;
scene->bars = ARENA_PUSH_ZERO_ARRAY(level, scene->bar_count, ArenaLabBar);
scene->build_serial = reload_count;
...
scene->phase_index = 0;
scene->cycle_count = 0;
scene->rewind_count = 0;
scene->manual_steps = 0;
scene->phase_timer = 0.0f;
scene->beat = 0.0f;
scene->peak_temp_bytes = 0;
arena_lab_apply_phase(scene);
```

That tells you the scene lifecycle clearly:

```text
fill function creates a fresh lab state
apply-phase chooses what the learner sees first
update code changes the phase later
```

## Step 5: Notice That the Bars Are Static but Their Meaning Is Dynamic

`game_fill_arena_lab(...)` populates the same four bars every rebuild:

- `perm` -> `session owner`
- `level` -> `scene rebuilds`
- `scratch` -> `frame-local work`
- `temp` -> `nested checkpoint`

These labels do not change per frame.

What changes is the scene's relationship to them during different phases.

That is a very good teaching pattern:

```text
keep the big conceptual landmarks stable
change the transient state around them
```

## Step 6: Read `build_serial` as a Small but Important Proof Hook

`scene->build_serial = reload_count;`

This field exists so the scene can prove that a rebuild really happened.

It is a tiny scene-local bridge from the global reload count into the scene payload.

This is the same pattern the course uses elsewhere:

```text
do not only rebuild state
also leave evidence that rebuild happened
```

## Common Mistake: Thinking This Scene Teaches Real Arena Internals Directly

It does not re-implement `Arena` or `TempMemory`.

Those were taught in the allocator module.

This lab teaches something different:

```text
what those lifetime rules look like when turned into live scene behavior
```

So the right mental model is not “allocator source again.”

It is “allocator behavior made visible.”

## JS-to-C Bridge

This is like a dedicated interactive demo page for a state-management system. The core library was taught earlier. This scene exists to make the library's behavior visible, repeatable, and testable in one constrained runtime surface.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find `ArenaLabScene` and group its fields into render state, simulation state, proof metrics, and explanatory strings.
2. Find `ArenaLabBar` and `ArenaCheckpointViz` and explain why the lab needs both.
3. Find `game_scene_summary(GAME_SCENE_ARENA_LAB)` and explain what promise it makes to the learner.
4. Find `game_fill_arena_lab(...)` and explain which fields are reset on every rebuild.
5. Explain why `build_serial` is useful even in a scene that mostly teaches temp lifetimes.

## Exercises

1. Explain why the Arena lab is a good first deep-dive scene after lesson 45.
2. Explain why the scene struct contains explanatory strings instead of leaving all wording to generic HUD code.
3. Explain why the four lifetime bars should stay stable across phases.
4. Describe the Arena Lab scene contract in one paragraph.

## Runtime Proof Checkpoint

At this point, you should be able to explain what the Arena Lab is made of before it starts running: one scene payload, one set of lifetime bars, one checkpoint visualization array, one small cluster of proof metrics, and one boot path that turns the allocator vocabulary into a teachable scene.

## Recap

This lesson established the Arena Lab as a scene-specific teaching surface:

- the scene ID, summary, and action hint define its contract
- `ArenaLabScene` stores render state, simulation state, and proof metrics together
- the fill path installs a fresh scene payload in the level arena
- the lifetime bars are stable landmarks while transient checkpoint state changes around them

Next, we open the phase machine that turns that static setup into a sequence of nested temp scopes, rewinds, and handoff states.
