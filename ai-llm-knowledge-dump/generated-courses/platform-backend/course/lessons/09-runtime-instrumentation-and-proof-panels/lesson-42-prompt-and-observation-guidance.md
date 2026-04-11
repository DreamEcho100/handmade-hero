# Lesson 42: Prompt Logic, Observation Logic, and Recovery-Aware Guidance

## Frontmatter

- Module: `09-runtime-instrumentation-and-proof-panels`
- Lesson: `42`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `progress_mask`
  - `required_mask`
  - `audio_warning_peak`
  - `game_audio_warning_recovered`

## Observable Outcome

By the end of this lesson, you should be able to explain why the runtime computes one line telling the learner what to do next and a separate line telling them what evidence to read afterward, and how those lines change when warning pressure is still active versus already recovered.

## Prerequisite Bridge

Lesson 41 built the proof-state model.

That still leaves a teaching question unanswered:

```text
how does the runtime turn proof state into actual guidance?
```

This lesson answers that through two different helper families:

- prompt logic
- observation logic

## Why This Lesson Exists

If the HUD only showed a static help block, it would say the same thing even after the learner had already proved a step.

This runtime wants something better:

```text
state-driven teaching guidance
```

So the HUD computes its guidance lines from live scene state, proof masks, and warning history.

## Step 1: Separate Action Guidance From Evidence Guidance

Before reading the helpers, compare these two sentence shapes.

### Prompt line

```text
Try now: press SPACE to inject a collision burst.
```

### Observation line

```text
Observe: a returned slot was reused while the pool backing stayed fixed.
```

These are not the same kind of instruction.

One asks for action.

The other asks for attention.

That is why the runtime keeps them separate.

## Step 2: Read `game_scene_exercise_prompt(...)`

This helper inspects:

- `progress_mask`
- `required_mask`
- current scene ID
- warning-peak and recovery state

and then returns the one action line the learner should see next.

It is effectively a small policy engine for guided interaction.

## Step 3: The Prompt Helper Has Two Big Modes

### Incomplete-proof mode

Point at the next missing step for the current scene.

### Complete-or-recovery mode

If all required steps are done, the helper shifts from "do this next" into completion or recovery guidance.

You can see that in the top of the function:

```c
if (required_mask != 0u && (progress_mask & required_mask) == required_mask) {
  if (game->audio_warning_peak[game->scene.current] >= 0.45f &&
      !game_audio_warning_recovered(game)) {
    return "Try now: stop pressing SPACE and watch severity cool ...";
  }
  if (game_audio_warning_recovered(game)) {
    return "Complete: you drove pressure up, then watched severity recover ...";
  }
}
```

That means the prompt system is not only step-based. It is also recovery-aware.

## Step 4: Read `game_audio_warning_recovered(...)`

The helper returns true when:

```c
game->audio_warning_peak[scene_id] >= 0.45f &&
game->audio_warning_heat[scene_id] <= 0.20f
```

This is an important concept.

The runtime wants to distinguish:

- whether pressure ever became significant
- whether that pressure has now cooled back down

That distinction lets the guidance system teach not only cause, but also recovery.

## Worked Example: Why Recovery Needs Its Own Guidance

Suppose the learner has already completed every required step in the pool lab and has also driven the warning line into yellow or red.

At that point, the next valuable lesson is no longer:

```text
cause more pressure
```

It is:

```text
stop causing pressure and watch the severity cool while the scene mechanics stay the same
```

That is exactly the state transition the prompt helper captures.

## Step 5: Read the Scene-Specific Prompt Branches as Action Scripts

After the completion/recovery preamble, the helper switches on the current scene and points the learner at the next missing proof.

Examples:

### Arena lab

- tap `SPACE` to take manual control of checkpoint stepping
- keep tapping until nested temp scopes are live
- continue until rewind begins
- wait for depth to fall back to zero

### Level-reload lab

- press `R` to rebuild the level arena
- watch rebuild flash settle
- wait for the scan head
- press `SPACE` to inject a scan-only spike

### Pool lab

- let the first fixed slot appear
- press `SPACE` to inject churn
- wait for a slot to return
- wait for a returned slot to be reused

### Slab lab

- let same-size events fill pages
- press `SPACE` to inject a burst
- keep pressure on until page growth occurs
- wait for flush/reuse behavior

That is a guided course sequence, not a static tooltip.

## Step 6: Read `game_scene_exercise_observation(...)`

This helper answers the second teaching question:

```text
after the learner acts,
what proof should they read from the runtime?
```

It inspects:

- current scene
- progress state
- scene-local metrics
- warning peak/recovery state

and returns one evidence-oriented sentence.

## Step 7: Observation Logic Is Also Recovery-Aware

At the top of the observation helper, if warning peak crossed the significance threshold, the text changes to explain cause versus severity.

For example:

- while still hot: watch how the cause label names the reason and the meter shows live severity
- after recovery: watch how the meter cooled even though the scene mechanics stayed the same

That is a stronger lesson than merely saying "warning happened."

It teaches:

```text
cause and severity are different concepts
```

## Step 8: Scene-Specific Observation Text Explains Proof, Not Action

The observation branches differ from the prompt branches in tone and purpose.

### Arena lab example

It explains that scratch returned to baseline, proving transient work did not escape.

### Level-reload example

It explains that rebuild flash means the strip was rebuilt, while later scan-only spikes reuse stable ownership.

### Pool lab example

It explains that slot reuse happened without moving the pool backing storage.

### Slab lab example

It explains that page growth happened under pressure, followed later by reuse of same-size slots.

These lines are not asking for a new action. They are teaching the meaning of what the learner just saw.

## Visual: Prompt Versus Observation Flow

```text
prompt line
  -> learner acts
  -> scene state changes
  -> observation line explains what that change proves
```

That is the real teaching loop.

## Step 9: Why These Helpers Return Strings Instead of Drawing Directly

This is a good architecture decision.

The helpers compute meaning.

The HUD composition layer draws that meaning.

That separation makes the code easier to reason about because:

- guidance logic stays independent from box layout and colors
- the same draw path can render whatever guidance string the policy returns
- text policy remains tied to live runtime state rather than to specific draw calls

## Practical Exercises

### Exercise 1: Explain the Split

What is the difference between a prompt line and an observation line?

Expected answer:

```text
the prompt line tells the learner what to do next, while the observation line tells them what evidence to notice after or during that action
```

### Exercise 2: Explain Recovery Awareness

Why do the helpers care about `audio_warning_peak` and `game_audio_warning_recovered(...)`?

Expected answer:

```text
because the runtime wants to teach not only how to create pressure but also how to recognize that pressure cooling back down, which is a different proof from the original cause
```

### Exercise 3: Explain String Helpers

Why is it useful that these helpers return strings instead of drawing directly?

Expected answer:

```text
because it keeps guidance policy separate from HUD rendering and lets the compositor reuse the current guidance text cleanly
```

## Common Mistakes

### Mistake 1: Treating prompt and observation as redundant text

They serve different roles in the teaching loop.

### Mistake 2: Thinking completion means guidance stops changing

The helper can still shift into recovery guidance after the core steps are complete.

### Mistake 3: Assuming the observation line is just a summary

It is the line that explains what the runtime evidence means.

## Troubleshooting Your Understanding

### "Why not just show the next incomplete step label and stop there?"

Because the runtime wants to coach the learner through both action and interpretation, not only list incomplete checklist items.

### "Why does warning recovery matter for teaching?"

Because understanding how a system cools back toward baseline is as important as understanding how to push it into a warning state in the first place.

## Recap

You now understand the dynamic guidance layer:

1. prompt logic chooses the next action
2. observation logic explains the meaning of the resulting evidence
3. both helpers are driven by live proof state and warning state
4. recovery-aware branches teach cause-versus-severity behavior explicitly

## Next Lesson

Lesson 43 moves from guidance text to the main trace panel itself: shared panel framing, step-row rendering, evidence suffixes, and the scene-aware composition that turns proof state into a visible checklist with live evidence attached.
