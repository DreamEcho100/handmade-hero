# Lesson 44: Audio Trace Panels, Cause Text, and the Warning Meter

## Frontmatter

- Module: `09-runtime-instrumentation-and-proof-panels`
- Lesson: `44`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_scene_audio_trace_title`
  - `game_describe_scene_audio_trace`
  - `game_draw_audio_trace_panel`
  - `game_draw_audio_warning_meter`
  - `game_describe_audio_anomaly`
  - `game_describe_audio_warning_context`
  - `game_describe_audio_warning_meter`

## Observable Outcome

By the end of this lesson, you should be able to explain how the audio trace panel separates scene-specific evidence, likely cause, and current severity instead of collapsing all audio warning behavior into one generic light or bar.

## Prerequisite Bridge

Lesson 43 ended with the main trace panel and noted that an audio trace panel is attached beneath it.

Lesson 44 explains that lower panel directly.

The key question is:

```text
how does the runtime explain audio pressure clearly enough
that the learner can tell cause, evidence, and severity apart?
```

## Why This Lesson Exists

If the HUD only showed a red/yellow/green light, it would hide too much meaning.

This runtime wants the learner to answer three separate questions:

1. what scene-specific audio evidence is visible right now?
2. what is the likely cause of the warning state?
3. how severe is that warning state at this moment?

That is why the audio trace panel has multiple textual layers plus a visual meter.

## Step 1: Read `game_scene_audio_trace_title(...)`

The title helper maps scene ID to a scene-aware panel label:

- arena -> `Audio Trace: checkpoint cues`
- level -> `Audio Trace: scan vs rebuild`
- pool -> `Audio Trace: voice churn`
- slab -> `Audio Trace: transport + sequencer`

This matters because the panel is not trying to be a generic audio dashboard.

It is trying to explain the current scene's audio meaning.

## Step 2: Read `game_describe_scene_audio_trace(...)`

This helper generates the first two lines of the panel.

Those lines are scene-specific evidence lines.

Examples on this branch:

### Arena lab

- active/free voices
- phase name
- cue depth
- rewind count
- sequencer step and tone volume

### Level-reload lab

- current pattern and step
- rebuild flash
- scan progress
- music and SFX volume
- voice count and highlighted entity

### Pool lab

- active/free voices
- oldest age
- hottest slot
- collision count, bursts, reuse hits, SFX volume

### Slab lab

- transport mode
- slab page count
- growth count
- event count
- sequencer step and tone state

These two lines are how the panel keeps audio evidence connected to scene behavior.

## Step 3: Separate Scene Evidence From Warning Interpretation

This is one of the most important ideas in the module.

The scene evidence lines answer:

```text
what is happening in the scene and audio system right now?
```

They do not yet answer:

```text
is this healthy, warming up, or actively bad?
why?
```

That second layer comes from the anomaly/cause/meter helpers.

## Step 4: Read `game_describe_audio_anomaly(...)`

This helper produces the anomaly text line.

It interprets the current scene plus live audio metrics and emits a sentence such as:

- warning about nested checkpoint cues saturating the pool
- warning that rebuild flash plus scan spike crowded voices
- warning that collision churn heated the pool
- warning that transport burst crowded sequencer headroom

This is the runtime's first explicit interpretation layer.

It is not raw metrics anymore. It is diagnosis text.

## Step 5: Read `game_describe_audio_warning_context(...)`

The helper builds a short cause-and-severity-context line using:

- current anomaly level
- current warning heat
- `game_audio_warning_cause_label(game)`

Its output distinguishes cases like:

- `severity:red live pressure`
- `severity:yellow accumulating risk`
- `severity:cooling after burst`
- `severity:green readable`

This is a crucial teaching choice.

The runtime is separating:

```text
what caused the pressure
from
how severe it is right now
```

## Worked Example: Cause Versus Severity

In the pool scene, the cause label may still be about churn and free-slot pressure.

But the severity can move through:

- green
- yellow
- red
- cooling back toward green

That means cause and severity do not move in lockstep.

The panel teaches that explicitly instead of hiding it.

## Step 6: Read `game_describe_audio_warning_meter(...)`

This helper formats the textual meter line using the heat/peak numbers.

Thresholds on this branch are:

- `heat >= 0.85` -> red linger
- `heat >= 0.45` -> yellow cooling/pressure
- otherwise -> clear

And the text also includes the stored `peak` value.

That means the runtime is not only showing current heat. It is also preserving recent history.

## Step 7: Why Peak Memory Matters

If the HUD only showed current heat, short spikes could disappear before the learner understood them.

By storing and displaying both:

- current `heat`
- recent `peak`

the runtime can teach:

```text
this system was recently stressed,
and now you are watching it cool back down
```

That is a much better debugging lesson than a purely instantaneous indicator.

## Step 8: Read `game_draw_audio_warning_meter(...)`

The meter renderer does two things.

### First

Draw a fixed background bar.

### Second

Draw a filled foreground bar with width `5.74f * heat` and color chosen by threshold.

Threshold coloring on this branch is:

- green below `0.45`
- yellow from `0.45` to below `0.85`
- red at `0.85` and above

That means the visual meter is tightly coupled to the same threshold model the text helpers use.

## Visual: Audio Trace Composition

```text
title
line 1: scene-specific audio evidence
line 2: more scene-specific audio evidence
line 3: anomaly interpretation
line 4: cause + severity context
line 5: pressure-memory text (heat + peak)
meter : visual severity bar
```

That is much richer than one warning light.

## Step 9: Read `game_draw_audio_trace_panel(...)` as the Audio-Explanation Compositor

The compositor:

1. draws the panel background and title strip
2. asks the scene-evidence helper for two lines
3. asks the anomaly helper for one line
4. asks cause-context and meter-text helpers for two more lines
5. draws the warning meter beneath them

This is the audio equivalent of the main trace-panel composition pattern from Lesson 43.

## Practical Exercises

### Exercise 1: Explain Scene Titles

Why does `game_scene_audio_trace_title(...)` use different titles for each scene instead of one generic audio title?

Expected answer:

```text
because the panel is supposed to explain the audio meaning of the current scene specifically, not only show detached audio subsystem stats
```

### Exercise 2: Explain Cause Versus Severity

Why does the panel need both `game_describe_audio_warning_context(...)` and the visual warning meter?

Expected answer:

```text
because the context line explains what kind of pressure is happening and how to interpret it, while the meter gives a fast visual read of how severe that pressure is right now
```

### Exercise 3: Explain Peak Memory

Why does the textual meter line include both current heat and peak?

Expected answer:

```text
because peak preserves recent stress history so the learner can see that a burst happened even after the current heat has already started cooling
```

## Common Mistakes

### Mistake 1: Treating the audio panel as generic debug output

It is scene-aware teaching output.

### Mistake 2: Confusing evidence lines with diagnosis lines

The first two lines are metrics; the later lines interpret those metrics.

### Mistake 3: Thinking the meter alone is enough

A bar can show severity, but not cause or scene-specific meaning.

## Troubleshooting Your Understanding

### "Why not merge anomaly and cause-context text into one line?"

Because the runtime wants one line that explains the specific situation and another that standardizes cause/severity interpretation. Keeping them separate makes the panel easier to scan.

### "Why do the thresholds appear in both text and drawing code?"

Because the textual interpretation and visual meter need to agree on what counts as green, yellow, and red pressure.

## Recap

You now understand the audio trace panel:

1. scene-aware titles and evidence lines keep the panel tied to current scene behavior
2. anomaly text interprets the current audio situation
3. cause-context text separates why pressure exists from how severe it is
4. the meter and meter-text line show current heat plus recent peak memory

## Next Lesson

Lesson 45 closes Module 09 by showing how the top HUD summary lines, allocator stats, scene-machine state, trace panel, and audio trace panel all fit together into one coherent proof-oriented overlay.
