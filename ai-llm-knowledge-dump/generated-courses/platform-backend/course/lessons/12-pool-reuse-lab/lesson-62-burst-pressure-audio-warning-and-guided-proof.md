# Lesson 62: Burst Pressure, Audio Warning, and Guided Proof

## Frontmatter

- Module: `12-pool-reuse-lab`
- Lesson: `62`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_scene_trace_step_evidence`
  - `game_scene_audio_trace_title`
  - `game_describe_scene_audio_trace`
  - `game_scene_audio_anomaly_level`
  - `game_scene_audio_pressure_score`
  - `game_audio_warning_cause_label`
  - `game_fire_scene_warning_probe`

## Why This Lesson Exists

Lesson 61 explained the visible proof surfaces.

This lesson explains how the runtime guides the learner through those surfaces in the right order.

The Pool scene is harder than the earlier labs in one important way: the most important proof event, reuse, often happens later than the user action that created extra pressure. The scene therefore needs prompts, trace evidence, observations, and warning language that teach the learner how to wait for the right event instead of concluding too early.

## Observable Outcome

By the end of this lesson, you should be able to explain the Pool exercise flow from prompt to observation to trace evidence to audio warning, and show how all of those surfaces are teaching the same delayed free-list story.

## First Reading Strategy

Read the guidance system in this order:

1. prompt text for what the learner should do next
2. exercise progress conditions for what actually marks a step complete
3. observation text for how the runtime interprets what just happened
4. trace evidence suffixes for the concrete metrics behind each step
5. audio trace and warning text for the pressure companion story

If you skip the progress conditions, the prompt flow can look more magical than it really is.

## Step 0: Start With the Actual Four-Step Sequence

The Pool trace panel names the sequence directly:

1. `allocate a fixed slot`
2. `collide and age out under churn`
3. `return slot to the free list`
4. `reuse the hottest returned slot`

That order is doing real pedagogical work. It walks the learner from current occupancy to churn, from churn to returned capacity, and only then from returned capacity to actual reuse.

The scene is teaching a temporal rule:

```text
pressure now does not necessarily mean reuse now
```

## Step 1: Read the Progress Conditions Beside the Prompts

`game_update_scene_exercise_progress(...)` marks the Pool steps using live fields:

- step 1 when `scene->active_count > 0`
- step 2 when `scene->collision_count > 0`
- step 3 when `scene->last_freed`
- step 4 when `scene->reuse_hits > 0`

This is a good teaching design because the prompts are not loose aspirations. Each one has a concrete runtime condition behind it.

## Step 2: Notice Why Step 1 Usually Completes Almost Immediately

`game_fill_pool_lab(...)` calls:

```c
pool_lab_spawn(scene, 6);
```

So the learner usually enters the scene with live probes already visible, and step 1 can complete quickly once the runtime sees `active_count > 0`.

That is intentional. The scene wants to begin from visible occupancy, not from an empty pool waiting for the first interesting thing to happen.

## Step 3: Read the Prompt Text as a Timed Lab Manual

The Pool prompt flow effectively says:

- let the scene light the board with fixed-slot allocation
- press `SPACE` to inject a collision burst and warm the warning line
- keep watching until one probe dies and returns a slot
- keep waiting until a returned slot is reused on the board below

This is strong tutorial pacing. The code is teaching the learner that the decisive proof event is not always the first dramatic thing they see.

## Step 4: Read the Observation Text as the Interpretation Layer

For the Pool scene, `game_scene_exercise_observation(...)` does two especially useful things.

Before reuse appears, it explains that collisions should eventually free a slot and make reuse visible on the board.

After reuse appears, it says:

```text
a returned slot was reused; pool backing stayed fixed while occupancy changed
```

That sentence is the allocator lesson in plain language.

- fixed storage stayed fixed
- occupancy changed over time
- reuse happened within the existing backing store

This keeps the learner from overfocusing on motion and underfocusing on ownership.

## Step 5: Read the Trace Evidence Suffixes as Proof, Not Decoration

For Pool steps, `game_scene_trace_step_evidence(...)` appends:

- `active=%d`
- `hits=%u`
- `free=%zu`
- `reuse=%u`

Those numbers match the conceptual ladder almost perfectly.

- occupancy is visible first
- collisions prove churn started
- free count proves capacity returned
- reuse count proves the cycle completed

The trace panel is therefore not just checking boxes. It is exposing the live fields that justify the boxes.

## Worked Example: Reading a Delayed Reuse Correctly

Suppose you press `SPACE` and immediately see more probes.

What should you conclude?

Not that reuse happened already.

The correct reading is:

1. burst pressure increased
2. collisions are now more likely
3. one or more probes may return to the free list soon
4. only after a later allocation returns a freed slot should `reuse_hits` prove immediate reuse

That delay is part of the lesson, not a flaw in the scene.

## Step 6: Read the Audio Trace as a Pressure Companion

The Pool audio title is:

```text
Audio Trace: voice churn
```

The scene-specific trace lines include:

```c
"voices:%d free:%d oldest:%d hottest_slot:%d"
"collisions:%u bursts:%u reuse:%u sfx:%.2f"
```

That means the Pool scene is teaching two related resource stories at once.

- the probe pool is under churn pressure
- the audio voice pool is also being crowded by the same bursty interaction pattern

This is why the audio surfaces matter. They help the learner see pressure as a system property, not just as a louder sound effect.

## Step 7: Read the Warning Logic as Scene-Specific Interpretation

For the Pool scene, the warning cause label is:

```text
collision churn + low free voices
```

The anomaly and pressure scoring logic then raise severity from things like:

- low free voice count
- collision count
- burst count

So the warning line is not generic loudness feedback. It is scene-specific interpretation of saturation pressure under churn.

## Step 8: Notice How `SPACE` Connects Every Surface at Once

On this scene, pressing `SPACE` does not only add probes. It also:

- increments `burst_count`
- records an event-log entry
- plays a warning-adjacent cue
- warms the audio-warning context
- increases the chance of later collisions and returned slots

That is what makes the Pool module feel like a guided proof instead of a free-play sandbox. One input affects occupancy, traces, logs, and pressure surfaces together.

## Common Mistake: Expecting Immediate Reuse After Every Burst

Not every burst should cause an instant reuse hit.

The scene is about churn over time. First there must be pressure, then a return, then a later allocation. The delay between those events is exactly what the prompts and observations are teaching you how to read.

## JS-to-C Bridge

This is like stress-testing a worker pool while also watching a queue-pressure alarm. One surface tells you what the resources are doing. The other tells you how close the surrounding system is to saturation.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read the Pool prompt sequence and explain the four-step learning order.
2. Explain why step 1 often completes immediately after scene start.
3. Read the Pool observation text and explain the phrase `pool backing stayed fixed while occupancy changed`.
4. Read the Pool trace evidence and explain how each suffix matches one conceptual step.
5. Read the Pool audio trace lines and warning cause label and explain how they reinforce the same churn story from a different angle.

## Exercises

1. Explain why delayed reuse is pedagogically useful in the Pool scene.
2. Explain how the trace evidence fields match the four conceptual steps of the scene.
3. Explain why Pool audio warning text is about churn and low free voices rather than rebuild or transport state.
4. Describe what pressing `SPACE` proves in the Pool scene beyond `more probes appeared`.

## Runtime Proof Checkpoint

At this point, you should be able to explain the Pool scene’s guided proof loop: prompts tell you what to wait for, progress conditions define what counts as success, observations interpret the meaning of what happened, trace evidence names the supporting metrics, and the audio warning system mirrors the same pressure story.

## Recap

This lesson explained the Pool scene’s guided proof surfaces:

- prompts and progress rules teach the temporal order of reuse
- observations translate raw events into allocator meaning
- trace evidence ties each step to a live field in the scene
- the audio trace and warning line present churn as system pressure rather than pure sound output
- deliberate burst input links all of those surfaces together

Next, we lock the scene and turn the entire Pool module into a deliberate verification procedure the learner can run end to end.
