# Lesson 69: Forced-Scene Walkthrough and Slab Growth Proof Checks

## Frontmatter

- Module: `13-slab-audio-lab`
- Lesson: `69`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `--force-scene`
  - `--lock-scene`
  - `COURSE_FORCE_SCENE_INDEX`
  - `COURSE_LOCK_SCENE`
  - `GAME_SCENE_SLAB_AUDIO_LAB`
  - `slab_alloc`
  - `slab_free`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`

## Why This Lesson Exists

The previous five lessons explained the Slab + Audio Lab from the inside.

This lesson turns that explanation into a live proof workflow.

The learner should now be able to prove four concrete claims:

- same-size events initially fit inside one slab page
- burst and transport pressure can force a new page
- page growth is visible as allocator growth, not only as audio heat
- later transport states keep reusing same-size slots inside that grown page set

## Observable Outcome

By the end of this lesson, you should be able to run the Slab + Audio Lab in isolation and prove, with live evidence, the full slab sequence from one-page occupancy to burst pressure to allocator growth to later reuse across the expanded page set.

## First Reading Strategy

Treat this lesson like an operator checklist.

1. lock the runtime to the slab scene
2. confirm the one-page baseline
3. add burst pressure deliberately
4. wait for real growth evidence
5. keep watching until the later flush state proves post-growth reuse

If you stop when a new page appears, you will miss the module's main claim about what happens afterward.

## Step 0: Use Isolation for a Reason

Use a forced scene build so every visible change belongs to the slab lesson:

```bash
./build-dev.sh --backend=raylib --force-scene=3 --lock-scene -r
```

Scene index `3` is the Slab + Audio State Lab. `--lock-scene` prevents the walkthrough from drifting into a different proof story.

## Step 1: Confirm the Baseline Before Pressing `SPACE`

At startup, verify:

- scene name: `Slab + Audio State Lab`
- summary about slab pages plus audio-state transitions
- top HUD line showing `mode`, `events`, `pages`, `growth`, `bursts`, `phase_swaps`
- trace panel with the four Slab steps
- audio trace title: `Audio Trace: transport + sequencer`
- visible first-page occupancy already present from seeded events

This baseline matters because the learner should see that the scene starts with same-size traffic already alive.

It also matters because the initial state gives you a control case. Before pressing anything, you should already know how one-page occupancy looks when the scene is healthy and readable.

One subtle source fact is worth keeping in mind here: `game_fill_slab_audio(...)` resets `prev_page_count` after the seeded eight-event fill. So the later growth proof is measuring pages beyond this boot baseline, not counting the initial page as growth.

## Step 2: Pass 1, Prove Initial Same-Size Occupancy

Do nothing for a moment.

Verify:

- step 1 is satisfied by visible recorded events
- event cards are already present
- one page board is visible with occupied slots
- current transport state is readable from the top blocks

This proves the scene starts from real same-size slab traffic, not an empty placeholder.

If the learner cannot read this baseline, the later growth event will be much harder to interpret.

## Step 3: Pass 2, Inject Burst Pressure With `SPACE`

Press `SPACE` once.

Verify:

- step 2 marks complete
- `burst_count` increases
- the event log records a slab burst
- warning severity warms up
- the cause label names `transport burst + sequencer layering`

This is the deliberate stress pass.

It should make the scene feel crowded without yet being mistaken for proof of page growth by itself.

That distinction is worth saying out loud during the walkthrough: burst pressure is the setup, not the verdict.

## Step 4: Pass 3, Stay With the Scene Until a New Page Appears

Keep pressure on the scene and wait through transport-state changes.

Verify:

- `page_growth_count` becomes non-zero
- step 3 marks complete
- the event log records `slab grew to ... pages`
- the page boards show another page
- the HUD line and trace both agree about page count

This is the allocator growth proof.

A new page exists now.

The strongest version of this step is when several surfaces agree at once: the HUD number changes, the trace step completes, the log records growth, and a second board appears visually.

## Step 5: Pass 4, Wait for the Later Reuse-Friendly State

Do not stop at page growth.

Keep watching until the transport machine cycles far enough that:

- `phase_changes >= 3`
- `state_index == 3`

In practice, that means the scene has advanced far enough to land in `flush`, because `slab_audio_state_name(3)` is exactly `flush`.

Then verify:

- step 4 marks complete
- the trace evidence appends `mode=...`
- the observation text shifts toward later reuse after growth
- page boards stay present while live occupancy inside them changes

That closes the real allocator lesson.

The system did not only grow.

It moved into a later phase where the existing pages keep getting reused.

That is why the walkthrough needs patience. The scene is teaching both the spike and the recovery behavior after it.

## Visual: The Minimal Slab Proof Loop

```text
boot locked scene
  -> read first-page occupancy
  -> press SPACE for burst pressure
  -> wait for page growth
  -> keep watching through later transport phases
  -> confirm reuse inside the grown page set
```

If the learner can explain that loop, the module is landing.

## Step 6: Connect Runtime Evidence Back to the Allocator Calls

By the end of the walkthrough, the learner should be able to map runtime evidence back to allocator operations:

- new event appears -> `slab_alloc(...)`
- page count increases -> allocator had to add another page
- event expires -> `slab_free(...)`
- later occupancy changes without new growth -> same-size slots are being reused

That is the real value of the scene.

The allocator is no longer invisible.

That is the point of forced-scene verification in this course. The runtime becomes a proving ground for low-level behavior, not just a place where the learner watches animations.

## Worked Example: Strong Versus Weak Verification

Weak verification:

```text
I pressed SPACE, the warning got hot, and eventually there were more pages.
```

Strong verification:

```text
the burst raised queue pressure and warning severity, but allocator growth was only proved
when the HUD, trace, page boards, and event log all showed a new page; later the flush state
kept changing occupancy inside those pages without requiring another growth spike
```

That stronger explanation is the standard this lesson is trying to train.

## Common Mistake: Treating the First New Page as the End of Verification

That is still too shallow.

The final proof standard is not only:

```text
I saw page count go up
```

It is:

```text
I saw growth happen,
then I watched later state changes reuse that expanded capacity
```

## JS-to-C Bridge

This is like verifying a page-based queue allocator under load: one burst forces another page, but the real proof is that later traffic reuses those pages instead of keeping growth unbounded.

## Try Now

Use the forced-scene build, then do these checks:

1. Start with `./build-dev.sh --backend=raylib --force-scene=3 --lock-scene -r` and confirm you boot directly into the Slab + Audio Lab.
2. Explain the initial one-page occupancy state before pressing `SPACE`.
3. Press `SPACE` once and explain which surfaces prove burst pressure increased.
4. Wait for page growth and explain which surfaces prove allocator growth, not only warning severity.
5. Keep watching through later transport changes and explain which surfaces prove same-size reuse after the growth spike.

## Exercises

1. Explain why the Slab walkthrough requires waiting through later transport states instead of stopping at the burst.
2. Explain why the page boards are necessary even though the HUD already shows `pages` and `growth`.
3. Explain how the final step proves reuse after growth rather than just growth alone.
4. Describe the full Slab walkthrough as an ordered proof procedure.

## Runtime Proof Checkpoint

At this point, you should be able to run the Slab + Audio Lab in isolation and prove, with live evidence, the full slab story from initial one-page occupancy through burst-driven page growth to later same-size reuse inside the expanded page set.

## Recap

This lesson closed the Slab + Audio module:

- the scene can be isolated with forced-scene builds
- `SPACE` injects transport burst pressure deliberately
- page growth is proved by logs, boards, trace fields, and HUD state together
- later transport phases prove reuse after the growth event

Next, the extension can move out of scene-specific labs and into the final integration block, where the runtime is treated as one diagnostic system rather than four separate teaching scenes.
