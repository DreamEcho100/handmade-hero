# Lesson 56: Render, Audio Trace, and Reload Proof Surfaces

## Frontmatter

- Module: `11-level-reload-lab`
- Lesson: `56`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_render_level_reload`
  - `game_scene_audio_anomaly_level`
  - `game_scene_audio_pressure_score`
  - `game_audio_warning_cause_label`
  - `game_describe_scene_audio_trace`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_scene_trace_step_evidence`
  - `game_draw_hud`

## Observable Outcome

By the end of this lesson, you should be able to explain how the Level Reload Lab proves rebuild-versus-spike behavior across several independent surfaces instead of trusting one visual effect alone.

## Why This Lesson Exists

Lesson 55 explained the behavior.

This lesson explains how the runtime proves that behavior.

That matters because the Level Reload Lab is deliberately tricky.

It uses one flash effect for two different events:

- real rebuild
- scan-only spike

So the learner needs multiple proof surfaces to avoid the wrong conclusion.

## First Reading Strategy

Read the proof surfaces from most spatial to most interpretive:

1. world render and bottom strip
2. top HUD metric line
3. trace labels and evidence suffixes
4. audio trace and warning context

That order mirrors how a learner would stabilize their interpretation.

## Step 0: Start With The Failure Mode

Before reading the proof surfaces, name the mistake the module is defending against:

```text
both R and SPACE can make the screen flash, so flash alone is not proof of rebuild
```

Every other surface in this lesson exists to protect the learner from that wrong conclusion.

## Step 1: Read the World Render as a Diagram of Ownership

`game_render_level_reload(...)` draws three key layers.

### Formation boxes

Each formation draws as a large background rectangle with its label.

That gives the scene its cluster headers.

### Entity rectangles

Each entity draws from the contiguous payload array.

The render highlights:

- current focus
- scan head contact
- per-entity heat

### Bottom memory strip

Then the scene draws the simplified strip row at the bottom and overlays the scan head highlight.

This is the most important teaching surface in the whole scene.

It turns a low-level ownership claim into a visible diagram.

## Visual: Three Proof Layers in One Scene

```text
top/middle world
  -> clusters and entities in scene space

bottom strip
  -> same entities as one contiguous row

hud/trace
  -> text proving rebuild, scan, and spike state
```

The render is not just pretty.

It is explanatory.

## Step 2: Notice the Built-In Teaching Labels

The render path literally writes:

```text
level arena rebuilds clusters into one contiguous entity strip
R rebuilds the whole strip, SPACE spikes the scan without reallocating
entity memory strip
scan head
```

That is unusually explicit.

The scene is not relying on the learner to infer the meaning of the picture.

It annotates the picture directly.

## Step 3: Read the Top HUD Metric Line Together With the Scene

In `game_draw_hud(...)`, the Level Reload metric line is:

```c
"Level formations:%d entities:%d seed:%u flash:%.2f scan:%.2f"
```

That line is important because it combines:

- payload size
- rebuild identity
- current visual rebuild/spike flash
- current scan progress

This makes the top HUD the compact summary while the world view shows the spatial meaning.

## Step 4: Read the Trace Panel as Guided Interpretation

The Level trace panel labels its four steps as:

1. `reset the level arena`
2. `rebuild formation headers`
3. `scan one contiguous entity strip`
4. `spike one entity without realloc`

Then `game_scene_trace_step_evidence(...)` appends live suffixes:

- `serial=%u`
- `flash=%.2f`
- `scan=%.2f`
- `focus=%d`

That is very strong proof design.

The panel does not only say what the learner should do.

It shows the live field that proves each step happened.

## Step 5: Read the Prompt and Observation Pair

For Level Reload, `game_scene_exercise_prompt(...)` and `game_scene_exercise_observation(...)` do something subtle.

The prompt moves the learner forward through:

- rebuild request
- rebuild evidence
- scan traversal
- spike comparison

The observation text then interprets the current state, for example:

```text
rebuild flash means the level arena was reset and the entity strip was rebuilt
SPACE should add audio pressure without changing ownership
```

This is the lesson in sentence form.

## Step 6: Read the Audio Trace as a Second Independent Proof System

For this scene, the audio trace title is:

```text
Audio Trace: scan vs rebuild
```

The scene-specific lines include:

```c
"pattern:%d step:%d flash:%.2f scan:%.2f"
"music:%.2f sfx:%.2f voices:%d focus:%d"
```

Then the warning logic specializes further.

### Anomaly level

`game_scene_audio_anomaly_level(...)` raises severity when:

- free voices hit zero
- or rebuild flash is high while voices are busy

### Pressure score

`game_scene_audio_pressure_score(...)` adds scene-specific pressure from:

- `rebuild_flash * 0.28f`
- `scan_t > 0.35f ? 0.12f : 0.0f`

### Cause label

`game_audio_warning_cause_label(...)` says either:

- `rebuild bed + scan spike`
- or `scan accent over rebuild`

That means the audio trace is not generic mixer telemetry.

It is a second interpretation layer for the same scene mechanics.

## Step 7: Explain Why This Scene Needs So Many Surfaces

If the learner only watched the world scene, they might think:

```text
flash means rebuild
```

But that is false after `SPACE`.

If the learner only watched the trace panel, they might miss the spatial meaning of the contiguous strip.

If the learner only watched the audio trace, they might miss that pressure is scene-specific and can warm up without allocation.

So the real proof comes from reading all surfaces together.

## Worked Example: Why The Bottom Strip Is More Than A Minimap

The bottom strip is not just a smaller copy of the entities on screen. It is the explicit teaching diagram for contiguity.

That is why the scan head label and strip caption matter so much: they turn one row of rectangles into proof that traversal is happening over one contiguous payload, not over three unrelated cluster-owned lists.

## Common Mistake: Treating the Audio Warning Line as a Performance Alarm Only

That is too narrow.

In this scene, the warning line is also a teaching device.

It helps the learner distinguish:

- rebuild-related pressure
- scan-only pressure
- current severity versus recent peak

## JS-to-C Bridge

This is like comparing a UI chart, a log line, and a profiler badge at the same time. One signal can mislead you. Three independent signals make the interpretation stable.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read `game_render_level_reload(...)` and explain why the bottom strip is the key ownership diagram.
2. Find the Level Reload HUD metric line and explain why `seed`, `flash`, and `scan` belong together.
3. Read the Level trace labels and evidence suffixes and explain what each proves.
4. Read the Level-specific audio trace lines and warning logic and explain how they reinforce the reload-versus-spike distinction.

## Exercises

1. Explain why the scene render alone is not enough to teach this lab correctly.
2. Explain how `game_scene_trace_step_evidence(...)` turns generic checklist boxes into proof surfaces.
3. Explain why the cause label changes from rebuild-heavy language to scan-heavy language.
4. Describe the role of the top HUD metric line in one sentence.

## Runtime Proof Checkpoint

At this point, you should be able to explain how the Level Reload Lab proves itself across several surfaces: the world render shows grouped payload and the contiguous strip, the HUD summarizes seed/flash/scan, the trace panel ties steps to live evidence, and the audio trace explains pressure without confusing it for allocator ownership.

## Recap

This lesson connected the Level Reload behavior to its proof surfaces:

- the world render visualizes grouped headers and one strip payload
- the HUD condenses rebuild and scan state into one line
- the trace panel maps each exercise step to a live field
- the audio trace adds an independent scene-specific pressure reading

Next, we lock the runtime into this scene and walk the learner through a full verification pass using forced-scene builds and deliberate `R`/`SPACE` checks.
