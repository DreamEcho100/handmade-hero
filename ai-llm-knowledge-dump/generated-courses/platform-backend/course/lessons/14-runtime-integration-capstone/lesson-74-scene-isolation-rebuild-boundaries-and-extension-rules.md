# Lesson 74: Scene Isolation, Rebuild Boundaries, and Extension Rules

## Frontmatter

- Module: `14-runtime-integration-capstone`
- Lesson: `74`
- Status: Required
- Target files:
  - `src/game/main.h`
  - `src/game/main.c`
- Target symbols:
  - `GameSceneID`
  - `GAME_SCENE_COUNT`
  - `GameLevelState`
  - `GameSceneMachine`
  - `game_scene_name`
  - `game_scene_summary`
  - `game_rebuild_scene`
  - `game_app_update`
  - `game_render_scene`
  - `game_scene_audio_trace_title`

## Why This Lesson Exists

The runtime feels like one program, but it is intentionally built from scene-local teaching worlds.

This lesson explains the isolation rules that make that possible.

Those same rules are also the handoff for anyone adding a fifth scene later.

## Observable Outcome

By the end of this lesson, you should be able to explain the runtime's scene-isolation contract: one level-state union is rebuilt per active scene, the level arena is reset at scene boundaries, and every learner-facing switch surface must be updated when the scene set grows.

## First Reading Strategy

Read the extension boundary from public API to private data:

1. `GameSceneID` and `GAME_SCENE_COUNT`
2. `GameLevelState` and its union
3. `game_rebuild_scene(...)`
4. update, render, and diagnostic dispatch switches
5. names, summaries, prompts, traces, and audio text helpers

That order shows why adding a scene is not a single edit. It is a contract update.

## Step 0: Build on the Diagnostic Lessons Correctly

Lessons 71 through 73 showed that the runtime exposes control state, render state, and diagnostic state honestly.

This lesson asks how the codebase preserves that honesty as the scene set evolves. The answer is strict isolation plus exhaustive wiring.

## Step 1: Start With the Public Scene Enum

`src/game/main.h` exposes `GameSceneID` with four current scene ids plus `GAME_SCENE_COUNT`.

That final count is not a convenience constant.

It sizes course-wide arrays such as:

- exercise progress per scene
- scene enter counts
- scene reload counts
- audio warning heat and peak values

So adding a new scene begins with the enum, but it does not end there.

`GAME_SCENE_COUNT` is part of the runtime contract because it determines how many parallel per-scene systems the app expects to manage.

## Step 2: Read `GameLevelState` as One Active Scene Union

`GameLevelState` stores:

- `scene_id`
- `scene_name`
- one union member for the active scene data

Only one of those union members is authoritative at a time.

That is the runtime's strongest isolation rule.

All scene-local state lives behind one current scene boundary.

This design is not only efficient. It prevents the course from accidentally carrying ghost state from one lab into another.

## Step 3: Read `game_rebuild_scene(...)` as the Isolation Reset Point

When the runtime changes or reloads a scene, it:

1. clears `game->level_state`
2. resets the level arena
3. allocates a fresh `GameLevelState`
4. fills only the current scene's union member

That means scene transitions are not incremental mutations across unrelated labs.

They are clean level-lifetime rebuilds.

This reset point is the architectural reason scene-local bugs stay easier to reason about. The runtime keeps a clean boundary between one active lab payload and the next.

## Step 4: Read Update and Render Dispatch as Exhaustive Scene Wiring

Once a scene exists, the runtime reaches it through switch statements:

- `game_app_update(...)`
- `game_render_scene(...)`
- helper functions that describe prompt, observation, trace, warning, and audio text

This is the practical extension rule.

If you add a new scene, you must wire it everywhere the runtime promises exhaustive scene behavior.

That exhaustiveness is what keeps the platform facade stable while the scene set grows behind it.

## Step 5: Read Names and Summaries as Public API, Not Decoration

Functions such as `game_scene_name(...)` and `game_scene_summary(...)` are part of the learner-facing contract.

If a new scene exists but those functions are not updated, the runtime can compile while its teaching surfaces fall out of sync.

That is why capstone extension work must include the diagnostic text layer, not only the raw scene logic.

The same rule applies to prompts, observations, trace labels, and audio trace titles. In this course, explanatory text is part of the runtime interface, not a postscript.

## Step 6: Use This Fifth-Scene Wiring Checklist

If you add another scene later, update at least these boundaries:

1. add the new id to `GameSceneID` and keep `GAME_SCENE_COUNT` correct
2. add the new scene struct to the `GameLevelState` union
3. add scene name and summary text
4. add a fill function and wire it into `game_rebuild_scene(...)`
5. add update and render dispatch cases
6. add prompt, observation, trace, warning, and audio-trace cases
7. verify arrays indexed by scene id still cover the full count

That is the minimum honest integration checklist.

## Worked Example: A Half-Integrated Fifth Scene

Suppose you add a new enum value and a render case, but forget to update `game_scene_name(...)` and the trace helpers.

What happens?

1. the scene may compile and draw
2. the HUD may show stale or missing descriptive text
3. per-scene arrays may still assume the old count if `GAME_SCENE_COUNT` was not handled correctly
4. the runtime would technically run, but the teaching contract would be broken

That is why this lesson treats text and diagnostics as first-class integration work.

## Step 7: Explain Why Scene Isolation Helps Teaching

Because only one scene state is live at once:

- level lifetime stays clear
- scene-specific proof surfaces stay focused
- reload behavior stays reproducible
- logs and HUD text can talk about one active teaching problem at a time

This is not only a software architecture choice.

It is a teaching architecture choice.

## Common Mistake: Adding a Scene to the Enum and Stopping There

That produces a partially integrated runtime.

The code may compile while the HUD, traces, prompts, or rebuild path still assume only four scenes.

Capstone extension work must close those gaps.

## JS-to-C Bridge

This is like adding a new route and component state slice, but forgetting to update navigation labels, analytics names, and test coverage. The feature exists, but the system contract is incomplete.

## Try Now

1. Open `src/game/main.h` and explain why `GAME_SCENE_COUNT` affects more than iteration.
2. Open `src/game/main.c` and identify the `GameLevelState` union plus the functions that must stay exhaustive per scene.
3. Explain why `game_rebuild_scene(...)` resets the level arena before constructing the next scene.
4. Write out the minimum checklist you would follow to add a fifth scene without breaking the diagnostic layer.

## Exercises

1. Explain why union-backed level state is a stronger isolation boundary than leaving every scene struct alive at once.
2. Explain why scene names and summaries should be treated as part of the public runtime contract.
3. Explain why prompt and trace wiring matter just as much as update and render wiring in this course.
4. Describe the full path from scene enum entry to fully integrated scene runtime behavior.

## Runtime Proof Checkpoint

At this point, you should be able to explain the runtime's scene-isolation contract: one level-state union is rebuilt per active scene, the level arena is reset at scene boundaries, and every learner-facing switch surface must be updated when the scene set grows.

## Recap

This lesson turned scene isolation into an extension checklist:

- `GameSceneID` and `GAME_SCENE_COUNT` define the public scene set
- `GameLevelState` keeps only one scene payload authoritative at a time
- `game_rebuild_scene(...)` is the level-lifetime reset boundary
- update, render, and diagnostic switches must stay exhaustive
- adding a new scene means updating teaching surfaces as well as runtime logic

Next, we close the entire course with a final verification matrix and a concrete handoff for extending the runtime beyond the current four scene labs.
