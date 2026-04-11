# Lesson 57: Forced-Scene Walkthrough and Reload Proof Checks

## Frontmatter

- Module: `11-level-reload-lab`
- Lesson: `57`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/game/main.c`
- Target symbols:
  - `--force-scene`
  - `--lock-scene`
  - `COURSE_FORCE_SCENE_INDEX`
  - `COURSE_LOCK_SCENE`
  - `GAME_SCENE_LEVEL_RELOAD`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_scene_trace_step_evidence`
  - `game_draw_hud`

## Observable Outcome

By the end of this lesson, you should be able to run the Level Reload Lab in isolation and deliberately prove the difference between real payload rebuild and scan-only pressure.

## Why This Lesson Exists

The previous five lessons explained the Level Reload Lab from the inside.

This lesson turns that into an operator workflow.

The learner should now be able to sit down with the running scene and deliberately prove four different claims:

- reload really rebuilds level-owned payload
- rebuild flash is visible proof, not the whole story
- scan traversal happens over stable rebuilt data
- `SPACE` can heat the scene without reallocating anything

## First Reading Strategy

Read this lesson as an operator procedure:

1. isolate the lab with build flags
2. verify baseline proof surfaces
3. run the rebuild path
4. run the stable traversal pass
5. run the spike path and compare it against rebuild

That is the right mindset for the second full lab walkthrough.

## Step 0: Know What You Are Comparing Before You Start

Before running the scene, state the core comparison explicitly:

```text
R should change ownership evidence
SPACE should change pressure evidence without changing ownership evidence
```

That sentence is the whole walkthrough in miniature.

## Step 1: Start With a Forced Level-Reload Build

Use:

```bash
./build-dev.sh --backend=raylib --force-scene=1 --lock-scene -r
```

That build configuration matters for the same reason it mattered in the Arena module.

It removes scene-switching noise.

You want every visible state change to belong to the Level Reload Lab.

## Step 2: Confirm the Baseline Before Touching Anything

At startup, look for:

- scene name: `Level Reload Lab`
- scene summary about scene-local rebuild
- action hint mentioning `SPACE`
- top HUD line with `formations`, `entities`, `seed`, `flash`, `scan`
- trace panel showing the four Level steps
- audio trace title: `Audio Trace: scan vs rebuild`

This baseline matters because the learner should know what the stable proof surfaces are before injecting any input.

## Step 3: Pass 1, Prove a Real Rebuild With `R`

Press `R` once.

Verify:

- step 1 marks complete
- the event log records a manual level rebuild request
- `build_serial` advances
- `reload_seed` changes
- the scene visibly regenerates its cluster/entity layout
- rebuild flash rises to a fresh visible value

This is the real ownership change in the scene.

## Step 4: Pass 2, Watch Rebuild Evidence Settle Into Stable Traversal

Do not press anything for a moment.

Verify:

- step 2 is backed by visible rebuild flash
- the flash starts cooling toward zero
- the scan head walks the bottom strip
- step 3 eventually marks when scan evidence is high enough
- the observation text switches from rebuild emphasis to stable strip traversal

This pass matters because it separates:

```text
the rebuild event
```

from:

```text
the later read-only scan of the rebuilt data
```

## Step 5: Pass 3, Inject a Spike Without Reallocating

Now press `SPACE` once.

Verify:

- step 4 marks complete
- the event log records `level scan spike -> entity ...`
- `highlighted_entity` jumps forward
- rebuild flash rises again
- the audio warning line warms up
- `build_serial` does not advance
- `reload_seed` does not change

This is the most important comparison in the whole module.

The scene gets louder and brighter.

Ownership does not change.

## Visual: The Minimal Level Reload Proof Loop

```text
boot locked scene
  -> press R
  -> confirm new serial and seed
  -> wait
  -> confirm scan over stable strip
  -> press SPACE
  -> confirm pressure without rebuild
```

If the learner can explain that loop, the module is landing.

## Step 6: Repeat the Comparison Once More

One run is good.

Two runs are better.

Press `R` again after the spike and compare:

- serial before and after
- seed before and after
- scene layout before and after
- trace evidence before and after

That second comparison removes any lingering ambiguity about whether `SPACE` did hidden rebuild work.

## Step 7: Use the Scene as a Verification Standard

By the end of this walkthrough, the learner should be able to answer three different questions without guessing:

1. What field proves a real rebuild happened?
2. What field proves the scan is traversing stable data?
3. What field proves a spike happened without reallocating the scene payload?

Those answers should come from live scene evidence, not memory of the lesson text.

## Common Mistake: Stopping After the First Flash

That is not enough.

Both `R` and `SPACE` can cause flash.

The real test is whether the learner keeps reading the other surfaces:

- serial
- seed
- scan
- focus
- event log
- warning cause label

## Worked Example: Why The Second Rebuild Pass Matters

After a `SPACE` spike, a second `R` press is valuable because it refreshes the learner's contrast set.

They can compare, back to back:

- what changed on the spike
- what changed on the rebuild

That second comparison is often clearer than the first because the learner now knows exactly which fields to watch.

## JS-to-C Bridge

This is like comparing a real data refresh against a purely visual highlight effect. The screen can look active in both cases. The question is which evidence proves the data model actually changed.

## Try Now

Use the forced-scene build, then do these checks:

1. Start with `./build-dev.sh --backend=raylib --force-scene=1 --lock-scene -r` and confirm you are locked into the Level Reload Lab.
2. Press `R` once and explain which fields prove a real rebuild happened.
3. Wait for scan traversal and explain which surfaces prove the scene is reading stable rebuilt data.
4. Press `SPACE` once and explain which surfaces heat up while ownership fields stay unchanged.
5. Press `R` again and explain why the second rebuild closes the comparison.

## Exercises

1. Explain why `--force-scene=1 --lock-scene` is the right verification setup for this module.
2. Explain the difference between rebuild evidence and spike evidence in Level Reload terms.
3. Explain why the second rebuild pass is pedagogically useful.
4. Describe the full Level Reload walkthrough as an ordered proof procedure.

## Runtime Proof Checkpoint

At this point, you should be able to run the Level Reload Lab in isolation and prove, with live evidence, the difference between rebuilding the scene-owned payload and merely stressing that payload with scan-focused visual and audio pressure.

## Recap

This lesson closed the Level Reload module:

- the scene can be isolated with forced-scene builds
- `R` proves real level-arena teardown and rebuild
- waiting proves the scan traverses stable rebuilt data
- `SPACE` proves pressure can rise without ownership change

Next, we move to a third allocator story: the Pool Reuse Lab, where the interesting boundary is neither rewind nor rebuild, but the return and reuse of fixed-size slots from a free-list allocator.
