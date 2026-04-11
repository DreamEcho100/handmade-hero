# Lesson 51: Forced-Scene Walkthrough and Arena Lab Proof Checks

## Frontmatter

- Module: `10-arena-lifetimes-lab`
- Lesson: `51`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/game/main.c`
  - `src/game/audio_demo.c`
- Target symbols:
  - `--force-scene`
  - `--lock-scene`
  - `COURSE_FORCE_SCENE_INDEX`
  - `COURSE_LOCK_SCENE`
  - `GAME_SCENE_ARENA_LAB`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_draw_hud`
  - `game_fire_scene_warning_probe`

## Observable Outcome

By the end of this lesson, you should be able to run the Arena Lab in isolation and verify the full allocator proof path deliberately instead of passively watching the scene loop.

## Why This Lesson Exists

The previous five lessons explained the Arena Lab from the inside.

This lesson flips the perspective.

Now the question becomes:

```text
if you sit down with the running program,
how do you deliberately prove that you understood the Arena Lab?
```

That requires a concrete operator workflow.

## First Reading Strategy

Read this lesson as a lab procedure, not as architecture commentary:

1. isolate the scene with build flags
2. perform one pass at a time
3. verify each pass through multiple proof surfaces
4. explain the meaning of each completed step before moving on

That is the right mindset for the first full lab walkthrough.

## Step 0: Understand Why Isolation Comes First

Before doing the walkthrough, ask:

```text
what kinds of noise would make this lab harder to read if scene switching stayed live?
```

The answer is exactly why `--force-scene` and `--lock-scene` matter: they remove unrelated transitions so the learner can attribute every visible change to the Arena Lab itself.

## Step 1: Start With a Forced Arena-Lab Build

The build script supports:

- `--force-scene=N`
- `--lock-scene`

The recommended command for this lesson is:

```bash
./build-dev.sh --backend=raylib --force-scene=0 --lock-scene -r
```

That matters because it removes scene-switching noise.

You want the learner locked into the Arena Lab so every visible change belongs to this lab and not to navigation or override drift.

## Step 2: Understand What the Force/Lock Flags Really Buy You

They do not teach allocator behavior directly.

They create a controlled observation environment.

That is their value.

```text
force-scene -> boot directly into the lab you want
lock-scene  -> keep the runtime there while you test it
```

This is exactly the right setup for proof-oriented lessons.

## Step 3: Walk the Arena Lab in Four Deliberate Passes

### Pass 1: Baseline Orientation

Do nothing at first.

Look for:

- scene name and summary
- phase banner
- current depth meter
- current prompt and observation text
- top HUD metric line

At this moment, the learner should confirm the static vocabulary:

```text
perm / level / scratch / temp
```

before any manual interaction begins.

### Pass 2: Manual Control Proof

Tap `SPACE` once.

Verify:

- step 1 marks complete
- the prompt advances
- the event log records a manual checkpoint advance
- the scene phase advances immediately
- the cue sound fires

This proves the learner can distinguish manual stepping from passive looping.

### Pass 3: Nested Pressure Proof

Keep tapping `SPACE` until the lab reaches depth `>= 2`.

Verify:

- step 2 marks complete
- the depth meter shows at least two live bars
- checkpoint cards stack visibly
- the warning line starts warming
- the audio warning probe can fire

This is where the lab starts teaching crowding and pressure, not only control.

### Pass 4: Rewind and Return Proof

Continue through rewind and then wait for the scene to return to depth zero.

Verify:

- step 3 marks on rewind
- `rewind_count` increases
- step 4 marks when depth returns to zero
- the observation text switches to the “nothing escaped” interpretation
- the trace and metric line agree about the final state

That closes the actual allocator lesson.

## Visual: The Arena Lab Verification Loop

```text
baseline
  -> tap SPACE once
  -> confirm manual control
  -> keep stepping
  -> confirm nested depth
  -> continue to rewind
  -> confirm transient work disappears cleanly
```

That is the minimal proof path this scene is built around.

## Step 4: Read the Arena Lab as a Multi-Channel Test Surface

By now, you should notice that verifying one claim involves checking multiple surfaces at once:

- prompt text
- observation text
- trace-step state
- event log
- scene graphics
- top HUD metrics
- audio cues

That is not overkill.

It is what makes the course reliable.

If one surface seems ambiguous, another surface can confirm or contradict it.

## Worked Example: Why Step 4 Matters More Than Only Seeing A Rewind

It is not enough to observe a rewind phase once.

The stronger proof is that after rewind finishes, depth really returns to zero and the observation text can honestly say no transient checkpoint escaped.

That final return-to-baseline state is what proves the lifetime lesson, not the rewind animation by itself.

## Step 5: Explain Why This Scene Comes First in the Extension

The Arena Lab is the simplest of the four deeper labs because it has the clearest “before and after” lifetime story.

It teaches the learner how to read the extension modules:

- identify the scene contract
- identify the main progression loop
- identify the proof surfaces
- perform the required actions
- confirm the runtime evidence

That reading pattern will carry forward into the later lab modules.

## Step 6: Treat This Lesson as the First Module Checkpoint

If the learner cannot complete the Arena Lab and explain why each step matters, then the extension has not landed correctly.

So this lesson is also an authoring checkpoint.

The scene, the prompts, the trace panel, and the HUD metrics must agree strongly enough that the full walkthrough works without guesswork.

## Common Mistake: Thinking a Successful Step Completion Is Enough

It is not enough to see a green box in the trace panel.

The real question is:

```text
can you explain why the step completed,
what scene state changed,
and what evidence on screen proves it?
```

That is the actual standard of understanding for this extension block.

## JS-to-C Bridge

This is like using a locked interactive lab mode instead of casually browsing a demo. The point is not only that the state changes. The point is that you can deliberately trigger the change, isolate it, and verify it through several independent signals.

## Try Now

Use the forced-scene build, then do these checks:

1. Start with `./build-dev.sh --backend=raylib --force-scene=0 --lock-scene -r` and confirm you boot directly into the Arena Lab.
2. Tap `SPACE` once and verify the trace, prompt, log, and cue all react.
3. Continue stepping until depth reaches at least two and explain which visual and audio surfaces prove that pressure increased.
4. Continue to a rewind phase and explain which evidence proves speculative work was discarded.
5. Wait until depth returns to zero and explain why that final state matters more than only seeing a rewind happen.

## Exercises

1. Explain why `--force-scene` and `--lock-scene` are valuable for lesson verification, not only for debugging.
2. Explain the full Arena Lab walkthrough in the order a learner should perform it.
3. Explain why the Arena Lab needs multiple proof surfaces instead of one “correct/incorrect” signal.
4. Describe what it would mean for the Arena Lab to fail pedagogically even if the code still ran.

## Runtime Proof Checkpoint

At this point, you should be able to run the Arena Lab in isolation, trigger each required proof event, and explain the allocator lesson using live evidence from the scene, the HUD, the trace panel, the event log, and the audio layer together.

## Recap

This lesson closed the first extension module:

- the Arena Lab can be isolated with forced-scene builds
- the learner has a concrete end-to-end walkthrough
- each required step is backed by multiple visible proof surfaces
- the module now functions as both allocator reinforcement and a model for reading the later scene labs

Next, we move from transient scratch checkpoints to a different lifetime story: the Level Reload Lab, where the important boundary is no longer nested temp scopes, but the full teardown and rebuild of scene-owned level data.
