# Lesson 68: Transport Pressure, Warning Lines, and Guided Interpretation

## Frontmatter

- Module: `13-slab-audio-lab`
- Lesson: `68`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_scene_audio_trace_title`
  - `game_describe_scene_audio_trace`
  - `game_scene_audio_anomaly_level`
  - `game_scene_audio_pressure_score`
  - `game_audio_warning_cause_label`
  - `game_describe_audio_warning_context`
  - `game_describe_audio_anomaly`
  - `game_update_scene_exercise_progress`

## Why This Lesson Exists

Lesson 67 explained how the slab scene proves allocator behavior visually.

This lesson explains how the runtime teaches the learner to interpret those visuals without confusing three different ideas:

- what caused pressure
- how severe that pressure is right now
- what the allocator actually did in response

The slab scene needs this lesson because it layers allocator growth, transport-state changes, sequencer pressure, and later reuse all at once. Without guided interpretation, the learner could easily treat one hot warning line as the whole story.

## Observable Outcome

By the end of this lesson, you should be able to explain the slab scene's interpretation model: prompts move the learner from occupancy to burst to page growth to later `flush`, while the warning system names cause and severity without pretending to be allocator proof on its own.

## First Reading Strategy

Read the interpretation system in this order:

1. prompt text
2. exercise progress conditions
3. observation text
4. trace evidence suffixes
5. audio trace lines
6. warning cause, severity, anomaly, and pressure-memory text

That order matters because the slab scene has more simultaneous signals than the earlier labs. The teaching ladder comes first; telemetry only makes sense once you know which question each signal is answering.

## Step 0: Separate Three Different Questions

This scene asks the learner to keep three questions distinct:

```text
what caused pressure?
how severe is the pressure now?
what did the allocator actually do?
```

Lesson 68 exists because those answers are related but not identical.

## Step 1: Read the Slab Prompt Sequence as a Teaching Ladder

For `GAME_SCENE_SLAB_AUDIO_LAB`, `game_scene_exercise_prompt(...)` walks the learner through four steps:

1. let the scene record same-size events into the first slab page
2. press `SPACE` once to inject a burst of same-size transport events
3. keep pressure on the scene until the slab grows by another page
4. wait for the transport machine to reach `flush` so reuse follows the growth spike

This is a strong sequence because it does not stop at the dramatic event.

The learner is guided through:

- ordinary occupancy
- deliberate burst pressure
- allocator growth
- later reuse-friendly transport state

That final step is the crucial design choice. The scene wants growth to be read as a spike, not as the end of the allocator story.

## Step 2: Read the Observation Text as the Main Interpretation Layer

The slab observation text is doing real instructional work.

Before growth, it says pressure is still fitting in current pages and the next burst may force growth.

After growth, it says:

```text
allocator pressure forced a new page, but later states still reuse same-size slots while the warning line tracks audio crowding separately
```

That sentence is the heart of the module.

It explicitly separates:

- allocator growth
- later same-size reuse
- audio warning severity

This is the runtime correcting a common mistake in real time. A hot warning line does not prove that growth just happened, and page growth does not mean the warning must still be hot.

## Step 3: Read the Trace Evidence as One Proof Field Per Step

For the slab scene, `game_scene_trace_step_evidence(...)` appends:

- `events=%d`
- `bursts=%u`
- `pages=%zu`
- `mode=%s`

That is a deliberately clean ladder.

Each completed step is backed by one direct field:

- step 1 proves same-size traffic exists
- step 2 proves a burst happened
- step 3 proves another page exists
- step 4 proves the transport machine later reached `flush`

The slab scene is noisy on purpose, so the trace panel becomes a narrowing tool. It tells the learner which field matters for the current proof step instead of asking them to interpret every moving part at once.

## Step 4: Read the Audio Trace as Transport Plus Sequencer Context

The audio trace title for this scene is:

```text
Audio Trace: transport + sequencer
```

Then `game_describe_scene_audio_trace(...)` prints two slab-specific lines:

```c
"mode:%s pages:%zu growth:%u events:%d"
"pattern:%d step:%d step_left:%d tone:%.2f"
```

That pairing matters.

The first line ties transport mode directly to slab growth and live occupancy.
The second line keeps the sequencer state visible at the same time.

So the audio trace is not merely a sound meter. It is a bridge between allocator pressure and the musical system that is helping generate that pressure.

## Step 5: Read Cause, Severity, and Pressure Score Separately

For the slab scene, `game_audio_warning_cause_label(...)` always names the cause as:

```text
transport burst + sequencer layering
```

That answers the first question: what kind of runtime activity is crowding the system?

Then `game_scene_audio_anomaly_level(...)` answers the severity question.

For `GAME_SCENE_SLAB_AUDIO_LAB`, it turns red when either of these is true:

- `free_voices == 0`
- `burst_count > 0 && event_count >= 10 && active_voices >= 7`

It turns yellow when any of these are true:

- `free_voices <= 2`
- `page_growth_count > 0`
- `event_count > GAME_SLAB_SLOTS_PER_PAGE && layer_level >= 1.10f`

Then `game_scene_audio_pressure_score(...)` tracks lingering pressure by combining:

- active and free voice usage
- `event_count`
- whether page growth has happened
- whether a burst has happened

That is why the warning meter can still show cooling pressure after the worst spike has passed. The scene distinguishes live severity from residual pressure memory.

## Step 6: Read the Warning Strings as Debugging Language

`game_describe_audio_warning_context(...)` prints scene-specific status such as:

- `cause:transport burst + sequencer layering | severity:red live pressure`
- `cause:transport burst + sequencer layering | severity:yellow accumulating risk`
- `cause:transport burst + sequencer layering | severity:cooling after burst`
- `cause:transport burst + sequencer layering | severity:green readable`

Then `game_describe_audio_anomaly(...)` gives the learner a longer sentence:

- `warning: transport burst crowded sequencer headroom during %s`
- `watch: queue pressure is layering tightly with the sequencer (pages:%zu)`
- `status: transport pressure is visible without overwhelming the sequencer`

This is good instructional language because it cleanly separates:

- named cause
- current severity
- a sentence about what the runtime is doing right now

The learner is being trained to read system pressure like a debugger, not just like a player reacting to a flashy effect.

## Worked Example: Yellow Warning Without New Growth

Suppose the learner sees:

```text
cause:transport burst + sequencer layering
severity:yellow accumulating risk
pages:1
growth:0
```

The correct reading is:

1. pressure is rising right now
2. transport and sequencer activity are the named cause
3. allocator growth has not been proved yet
4. another burst or later state transition may still force another page

That is exactly the distinction this lesson is trying to build. Warning severity is a pressure signal. Page growth is a separate allocator fact.

## Step 7: Notice the Final Exercise Step Uses Phase, Not Only Memory

`game_update_scene_exercise_progress(...)` marks slab step 4 when:

```c
scene->phase_changes >= 3u && scene->state_index == 3u
```

Since `slab_audio_state_name(3)` is `flush`, the final proof step is intentionally not just:

```text
page count went up
```

It is:

```text
after growth, the transport machine later reached flush and the scene kept reusing the expanded page set
```

That is the strongest idea in the whole module.

## Common Mistake: Treating the Warning Line as Proof of Allocator Growth

It is not.

The warning line tracks audio pressure and recovery.

Allocator growth is proved by:

- `pages=%zu` in the trace and audio trace
- `page_growth_count`
- the event log line that says the slab grew
- the page boards in the world render

The scene deliberately keeps those proof channels separate.

## JS-to-C Bridge

This is like watching queue pressure, page growth, and scheduler headroom all move together in one runtime. A good engineer does not collapse them into one meaning. The job is to separate cause, severity, and allocator state correctly.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read the slab prompt sequence and explain the four-step learning order.
2. Read the slab observation text and explain why it separates allocator growth from warning severity.
3. Read the slab anomaly and warning helpers and explain what each one answers: cause, severity, or current runtime description.
4. Read the step-4 completion condition and explain why the module waits for `flush` instead of ending at the growth spike.

## Exercises

1. Explain why page growth alone is not the final proof event in this scene.
2. Explain how the slab warning strings teach cause-versus-severity separation.
3. Explain why `page_growth_count > 0` contributes to yellow pressure without automatically meaning red failure.
4. Describe the full guided interpretation path of the slab scene.

## Runtime Proof Checkpoint

At this point, you should be able to explain the slab scene's interpretation model: prompts lead the learner from occupancy to burst to page growth to later `flush`, the trace panel assigns one proof field per step, and the warning system explains pressure without replacing allocator proof surfaces.

## Recap

This lesson explained how the slab scene teaches interpretation:

- prompts push the learner through occupancy, burst, growth, and later reuse-friendly `flush`
- observations explain allocator growth separately from warning severity
- the audio trace pairs transport mode with sequencer and page data
- warning helpers separate cause, severity, and descriptive runtime status

Next, we turn the whole module into a locked-scene verification walkthrough so the learner can deliberately force pressure, watch a page appear, and then confirm later same-size reuse in the running program.
