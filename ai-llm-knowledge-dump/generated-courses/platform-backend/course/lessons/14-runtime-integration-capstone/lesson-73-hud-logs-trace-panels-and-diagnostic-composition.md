# Lesson 73: HUD, Logs, Trace Panels, and Diagnostic Composition

## Frontmatter

- Module: `14-runtime-integration-capstone`
- Lesson: `73`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_draw_hud`
  - `game_draw_scene_trace_panel`
  - `game_draw_audio_trace_panel`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_describe_scene_audio_trace`
  - `game_describe_audio_warning_context`
  - `game_log_eventf`

## Why This Lesson Exists

Earlier lessons explained individual panels one scene at a time.

This lesson treats the HUD as one integrated diagnostic surface.

That is the right final step because the runtime is no longer teaching only arena pages, reload strips, pools, or slabs.

It is teaching how to trust runtime evidence across the whole course.

## Observable Outcome

By the end of this lesson, you should be able to explain the full diagnostic composition model: shared summary lines describe platform-wide state, scene-local detail lines describe the active lab, logs preserve recent history, and the right-side trace panels guide proof and interpretation together.

## First Reading Strategy

Read the HUD in columns and layers:

1. shared left-column state
2. scene-specific detail line
3. recent log history
4. scene trace panel
5. audio trace panel and warning context

This order matters because the HUD is not one flat wall of text. Each area answers a different category of question.

## Step 0: Build on the Two-Context Model

Lesson 72 explained why the HUD stays readable at all.

This lesson asks what the HUD actually says once that stable panel space exists. The answer is that the HUD is not merely decorative status text. It is a deliberately layered diagnostic system.

## Step 1: Read the Left Column as Shared Runtime State

`game_draw_hud(...)` begins with information that is valid no matter which scene is active:

- scene name and scene summary
- action hint
- control help
- camera help
- current, requested, and previous scene plus transition count
- override state and current frame
- arena usage and audio summary

That left column is the runtime's common diagnostic spine.

It is the part of the HUD that keeps the learner oriented before any scene-specific proof begins.

## Step 2: Read the Scene-Specific Detail Line as the Current Local Truth

After the shared lines, the HUD prints one scene-specific yellow detail line.

That line changes by scene:

- arena depth and temp bytes
- level reload formations and scan flash
- pool active count and hottest slot
- slab mode, pages, growth, and bursts

This is a good capstone pattern.

The runtime gives you one stable shared language plus one local scene report.

That local line is especially useful because it compresses the current scene's most important state into one glance before the learner reads the deeper panels on the right.

## Step 3: Read the Log Ring as Temporal Evidence

Below those summary lines, `game_draw_hud(...)` prints recent log entries from the ring buffer.

Those logs capture transitions such as:

- scene enter and reload events
- slab growth
- burst triggers
- override toggles
- rebuild failure notifications

This matters because a single frame can only show current state.

The log ring preserves recent history.

That makes the HUD more than a snapshot. It becomes a short audit trail of how the current state came to be.

## Step 4: Read the Right Column as Guided Interpretation

The right side is not just decoration.

It holds the teaching surfaces that tell the learner what to prove next:

- the scene trace panel
- the exercise prompt
- the exercise observation
- the audio trace panel
- the warning context and meter

This is where the runtime stops being a raw demo and becomes a guided lab.

## Step 5: Read Scene Trace and Audio Trace Together

`game_draw_scene_trace_panel(...)` answers:

```text
which exercise step is active, and what evidence marks it complete?
```

`game_draw_audio_trace_panel(...)` answers:

```text
what is the audio subsystem doing while that scene evidence changes?
```

Those two panels belong together.

One explains proof progression.

The other explains concurrent audio state.

The capstone skill is learning to correlate them without collapsing them into one signal.

## Step 6: Notice the Failure-Mode Fallback Is Still Informative

If `level_state` is missing, the HUD does not try to fake scene detail.

Instead it prints:

- the rebuild warning line
- a message saying scene state is unavailable
- the recent log history

That is correct diagnostics behavior.

When live scene data is absent, the runtime narrows its claims instead of inventing placeholders.

This is one of the clearest examples in the course of honest diagnostics design.

## Step 7: Use the HUD as a Multi-Surface Cross-Check

The capstone reading rule is:

never trust only one surface when multiple surfaces should agree.

For example, a slab page-growth claim is strongest when it appears in:

- the scene detail line
- the exercise trace evidence
- the audio trace line
- the event log

That cross-checking habit is the real final skill.

## Worked Example: One Strong Runtime Claim

Suppose you want to prove pool reuse happened.

A strong verification does not rely only on one counter. It correlates:

1. the Pool detail line in the HUD
2. the trace panel's reuse step and appended evidence
3. the audio trace's collision and reuse context
4. the recent event log if a burst or milestone was recorded

That is how the runtime trains you to trust evidence as a system.

## Common Mistake: Treating the Warning Meter as the Whole Diagnostic Story

It is only one signal.

The runtime deliberately separates:

- control state
- memory state
- scene-local state
- audio state
- recent event history
- guided prompt and observation text

No single line replaces the rest.

## JS-to-C Bridge

This is like having devtools, logs, and a guided test checklist open at the same time. The point is not to pick one. The point is to correlate them.

## Try Now

1. Open `src/game/main.c` and identify which `game_draw_hud(...)` lines are shared across all scenes.
2. Explain why the scene-specific yellow line is useful even though the right-side trace panel already exists.
3. Explain how the log ring complements the current-frame HUD state.
4. Explain what the HUD does differently when `level_state` is unavailable.

## Exercises

1. Explain why recent logs are necessary even in a strongly instrumented HUD.
2. Explain the difference between the scene trace panel and the audio trace panel.
3. Explain why the runtime narrows its claims during rebuild failure instead of drawing stale scene details.
4. Describe how to verify a runtime claim using at least three HUD surfaces together.

## Runtime Proof Checkpoint

At this point, you should be able to explain the full diagnostic composition model: shared summary lines describe platform-wide state, scene-local detail lines describe the active lab, logs preserve recent history, and the right-side trace panels teach the learner what to prove and how to interpret concurrent audio behavior.

## Recap

This lesson unified the runtime's proof surfaces:

- the left HUD column provides shared state and controls
- one scene-specific line reports the active lab's local truth
- logs preserve recent transitions and anomalies
- the trace panels pair proof progression with audio interpretation
- rebuild-failure fallback stays informative without pretending scene data exists

Next, we leave observation and move into extension rules by showing exactly how scene isolation is enforced and what must change when you add another scene to the runtime.
