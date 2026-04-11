# Lesson 75: Final Verification Matrix and Custom Scene Handoff

## Frontmatter

- Module: `14-runtime-integration-capstone`
- Lesson: `75`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/game/main.h`
  - `src/game/main.c`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `--backend`
  - `--force-scene`
  - `--lock-scene`
  - `--bench`
  - `game_app_init`
  - `game_app_update`
  - `game_app_render`
  - `game_app_get_audio_samples`
  - `GameSceneID`

## Why This Lesson Exists

The course is finished only if the learner can verify the whole runtime, not just explain individual lessons.

This final lesson turns everything into one practical exit checklist.

It also hands off the exact boundary you would extend if you wanted to add a new scene after the course ends.

## Observable Outcome

By the end of this lesson, you should be able to verify the full course runtime end to end: both backends build the same game facade, forced-scene runs isolate each lab, bench mode measures named runtime regions, live controls still work together, and future custom scenes can be added behind the same platform boundary.

## First Reading Strategy

Treat this lesson like a signoff matrix rather than like theory.

1. verify backend parity
2. verify scene isolation
3. verify bench instrumentation
4. verify live controls and proof surfaces
5. verify the stable extension boundary

This lesson is the final compression of the whole course. The point is not to add new concepts. The point is to prove the current ones hold together.

## Step 0: Carry the Previous Lessons Forward Intentionally

Lessons 70 through 74 established the capstone seams: measurement, control policy, render composition, diagnostics, and scene isolation.

Lesson 75 closes the loop by turning those seams into an explicit operator checklist. That is the continuity bridge from the slab walkthrough: isolated scene proof was the training ground; runtime verification is the graduation test.

## Step 1: Verify the Two Real Platform Targets

The runtime is only complete if both supported backends still run the same game facade.

Use at least:

```bash
./build-dev.sh --backend=raylib
./build-dev.sh --backend=x11
```

The capstone claim is not merely that scene code works.

It is that the shared platform contract still carries the same runtime across both backends.

That matters because the stable handoff after the course is not one backend-specific executable path. It is a facade that both backends already trust.

## Step 2: Verify Locked-Scene Entry for Each Current Lab

Run focused builds that jump directly into each scene:

```bash
./build-dev.sh --backend=raylib --force-scene=0 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=1 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r
./build-dev.sh --backend=raylib --force-scene=3 --lock-scene -r
```

Those four runs prove that the scene machine, rebuild path, and forced override logic can all isolate the current labs on demand.

This is the runtime-wide version of the isolated proof workflows you practiced in Modules 10 through 13.

## Step 3: Verify One Timed Bench Run

Use:

```bash
./build-dev.sh --backend=raylib --bench=5 -r
```

That proves the runtime still supports:

- a measurement build personality
- profiler activation
- timed auto-exit
- summary printing from the platform loop

The capstone standard is not to memorize the output.

It is to know how to obtain it reliably.

## Step 4: Verify One Live Control Pass

In a normal run, confirm the runtime can still do all of these together:

- move between scenes
- reload the active scene
- toggle the HUD
- toggle and clear runtime override
- pan and zoom the camera
- trigger each scene's `SPACE` action

That is the final integration test because it crosses input, state machine logic, rebuilds, rendering, diagnostics, and audio response.

This is the point where the runtime stops being a set of lessons and proves it is one working system.

## Step 5: Verify One Multi-Surface Proof in Every Scene

Do not stop at entering the scene.

For each lab, prove one real claim using multiple surfaces together:

- Arena: temp-depth evidence plus logs plus trace progression
- Level Reload: rebuild flash or reload counters plus contiguous-strip evidence
- Pool: generation and reuse evidence plus slot-board state
- Slab: page growth or later reuse evidence plus logs plus trace fields

This proves the runtime is still a teaching system, not only a scene switcher.

## Verification Matrix: What To Run And What It Proves

| Verification area  | Command or action                                                              | What you should prove                                                          |
| ------------------ | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------ |
| backend parity     | `./build-dev.sh --backend=raylib` and `./build-dev.sh --backend=x11`           | both backends still drive the same `game_app_*` facade                         |
| scene isolation    | `--force-scene=N --lock-scene` runs for each current lab                       | scene requests, rebuild path, and override logic can isolate one lab at a time |
| measurement path   | `./build-dev.sh --backend=raylib --bench=5 -r`                                 | profiler hooks, timed exit, and measurement personality still work             |
| live integration   | normal run with navigation, reload, HUD, override, camera, and `SPACE` actions | input, scene machine, render, diagnostics, and audio still cooperate           |
| proof surfaces     | inspect one real claim in every scene                                          | the runtime still teaches through evidence, not only through successful entry  |
| extension boundary | inspect `game_app_init/update/render/get_audio_samples`                        | future scenes can be added behind the existing platform boundary               |

This table is the real signoff target for the whole course.

## Step 6: Read the Final Extension Boundary

If you continue beyond the course, the main public handoff is still the same four-function facade:

- `game_app_init(...)`
- `game_app_update(...)`
- `game_app_render(...)`
- `game_app_get_audio_samples(...)`

That is the contract both backends already trust.

If you add a custom scene, you extend the scene machine behind that facade rather than changing the platform boundary itself.

This is the cleanest closing rule in the course: the platform edge stays stable while the scene set evolves behind it.

## Visual: Final Verification Loop

```text
build both backends
  -> isolate each scene
  -> run one timed bench
  -> verify live controls
  -> prove one real claim per scene
  -> extend behind the same game_app facade
```

That loop is the clean end of the course.

## Worked Example: Strong Final Signoff

Weak signoff:

```text
it builds and I can switch scenes.
```

Strong signoff:

```text
both backends call the same game_app facade, each current scene can be isolated and verified,
bench mode still measures named runtime regions, live controls and HUD diagnostics remain consistent,
and a future fifth scene can be added behind the existing platform boundary
```

That stronger statement is what this lesson is trying to earn.

## Common Mistake: Ending With Only a Successful Build

A successful build is necessary, but it is not the capstone.

The capstone is successful verification:

- platform parity
- scene isolation
- measurement support
- guided proof surfaces
- a clear extension handoff

## JS-to-C Bridge

This is like shipping the final app only after smoke tests, focused route tests, and one measured performance pass all succeed, then documenting the stable public API you expect future features to preserve.

## Try Now

1. Build both backends and confirm they still target the same `game_app_*` facade.
2. Run the four forced-scene Raylib commands and explain what each one proves about the scene machine.
3. Run one short bench build and explain what it proves about runtime instrumentation.
4. Describe how you would add a fifth scene without changing the platform-facing game facade.

## Exercises

1. Explain why platform parity is part of the final verification matrix.
2. Explain why each scene should be verified with real proof surfaces rather than only successful entry.
3. Explain why the `game_app_*` facade is the stable extension boundary after the course ends.
4. Describe the complete final verification loop in order.

## Runtime Proof Checkpoint

At this point, you should be able to verify the full course runtime end to end: both backends build the same game facade, forced-scene runs isolate each lab, bench mode measures named runtime regions, live controls still work together, and future custom scenes can be added behind the same platform boundary.

## Recap

This lesson closes the full 75-lesson bridge course:

- both backends remain valid platform targets
- each scene can be isolated and proved directly
- bench mode gives the runtime one honest measurement path
- live controls still integrate input, rebuilds, rendering, HUD, and audio
- future extension work happens behind the existing `game_app_*` facade

From here, the course can stop teaching implementation details scene by scene and instead serve as a reusable platform-and-runtime foundation for future game-specific courses.
