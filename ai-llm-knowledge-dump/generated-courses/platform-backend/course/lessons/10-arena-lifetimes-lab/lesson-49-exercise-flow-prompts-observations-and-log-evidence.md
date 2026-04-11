# Lesson 49: Exercise Flow, Prompts, Observations, and Log Evidence

## Frontmatter

- Module: `10-arena-lifetimes-lab`
- Lesson: `49`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_mark_scene_exercise`
  - `game_update_scene_exercise_progress`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_scene_trace_step_evidence`
  - `game_log_eventf`
  - `GAME_EXERCISE_STEP_1`
  - `GAME_EXERCISE_STEP_2`
  - `GAME_EXERCISE_STEP_3`
  - `GAME_EXERCISE_STEP_4`

## Observable Outcome

By the end of this lesson, you should be able to explain how one Arena Lab interaction becomes a full proof event across prompts, observations, trace rows, evidence suffixes, and event-log lines.

## Why This Lesson Exists

Lessons 41-45 already explained the generic proof layer:

- step masks
- focus step
- prompts
- observations
- trace panels
- HUD composition

What they did not do was stay inside one scene long enough to show how all of that machinery hangs together during a concrete learner workflow.

This lesson does that for the Arena Lab.

## First Reading Strategy

Read the proof flow in the same order the learner experiences it:

1. one action happens
2. one proof step is marked or inferred
3. prompt and observation text update
4. trace evidence and log lines confirm what changed

That order keeps the generic proof layer anchored in one concrete scene.

## Step 0: Follow One Arena Step End To End

Start with the simplest complete story:

```text
press SPACE once
step 1 marks complete
prompt advances
event log records the action
trace panel shows the new proof state
```

That is the pattern the later steps repeat with different conditions.

## Step 1: Start With the Four Arena-Specific Steps

In practice, the Arena Lab's proof flow is:

1. take manual control of the checkpoint stack
2. reach at least two live temp scopes
3. enter a rewind phase
4. return depth to zero

That is a very tight lesson arc.

It teaches:

```text
manual action -> nesting -> rewind -> clean baseline return
```

This is why the scene is such a good allocator lab.

## Step 2: Notice That Step 1 Is Marked Directly in the Update Path

Inside `game_update_arena_lab(...)`, a fresh manual SPACE press calls:

```c
game_mark_scene_exercise(game, GAME_SCENE_ARENA_LAB, GAME_EXERCISE_STEP_1,
                         "manual checkpoint advance");
```

That means step 1 is tied to deliberate learner action.

It is not inferred later from a state summary.

This is a good distinction.

The scene is explicitly teaching:

```text
the first proof is that the learner took control
```

## Step 3: Read the Later Arena Steps in `game_update_scene_exercise_progress(...)`

The follow-up steps are marked centrally from scene state.

For the Arena Lab, the runtime checks for:

- depth `>= 2`
- rewind count `> 0`
- depth `== 0` after the earlier work

That split is important.

It means the system uses two kinds of proof sources:

- direct action marks
- derived state marks

That is how the exercise system stays both guided and robust.

## Step 4: Read the Arena Prompt Sequence as the Learner Script

In `game_scene_exercise_prompt(...)`, the Arena branch walks the mask in order:

- tap SPACE once
- keep tapping until two temp scopes are live
- continue stepping until rewind
- wait for rewind to finish and confirm depth returns to zero

This is not generic prompt text.

It is a scene-specific teaching script derived from real scene state.

That is why it works.

## Step 5: Read the Observation Text as the “What Should I Notice?” Layer

The Arena observation branch says different things depending on state.

Examples:

- when depth is nonzero: nested scopes are live and should later disappear cleanly
- when depth returns to zero: scratch is back at baseline and no transient checkpoint escaped
- after full completion: the learner is told the transient branch really was transient

This is a subtle but very important design choice.

The prompt tells the learner what to do.

The observation tells the learner what the result should mean.

## Visual: Prompt vs Observation

```text
Prompt       -> perform this action now
Observation  -> interpret the visible result correctly
```

That separation is one of the strongest pedagogical patterns in the whole runtime.

## Step 6: Read the Arena Step Evidence Suffixes

`game_scene_trace_step_evidence(...)` adds Arena-specific proof suffixes:

- step 1 -> `manual=%u`
- step 2 -> `depth=%d`
- step 3 -> `rewinds=%u`
- step 4 -> `depth=%d`

These are small strings, but they matter.

They let the trace panel say not only:

```text
this step is done
```

but also:

```text
here is the live metric that proves it
```

That is a big quality difference.

## Worked Example: Why Arena Step 1 Is Marked Directly

Step 1 is the learner's proof of intentional control, so the runtime marks it directly on manual input instead of waiting to infer it from later state.

That matters because the scene wants to teach:

```text
the learner deliberately took ownership of the checkpoint script
```

The later steps then switch to state-derived proof because those steps are about observed behavior rather than button ownership.

## Step 7: Keep the Event Log in the Picture

The exercise and phase system also writes human-readable log lines through `game_log_eventf(...)`.

Examples include:

- arena phase changes
- exercise-step completions
- exercise-complete line
- audio-probe lines

So the proof layer for this scene is actually multi-channel:

- trace step state
- prompt and observation text
- HUD metric line
- event log
- audio cues

That density is exactly what makes the lab valuable.

## Common Mistake: Thinking the Exercise System Only Checks Completion

It is doing much more than completion bookkeeping.

It is also deciding:

- which prompt is current
- which observation is most relevant
- which step gets focus
- which evidence suffix appears
- when to log milestone events

That is why scene-specific exercise logic deserves its own deep-dive lesson.

## JS-to-C Bridge

This is like a guided interactive tutorial where each step has an action, a proof condition, a current hint, a post-action interpretation, and an audit trail entry. The difference is that here those pieces are all driven from live runtime state rather than from a scripted slideshow.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find the direct call that marks Arena step 1 on manual input.
2. Find the Arena branch inside `game_update_scene_exercise_progress(...)` and explain which conditions mark steps 2, 3, and 4.
3. Read the Arena branch of `game_scene_exercise_prompt(...)` and explain how the prompt evolves over the full scene.
4. Read the Arena branch of `game_scene_exercise_observation(...)` and explain how it changes when depth is zero versus nonzero.
5. Find the Arena branch of `game_scene_trace_step_evidence(...)` and explain why the suffixes are useful.

## Exercises

1. Explain why step 1 is action-driven while later steps are state-driven.
2. Explain why prompt and observation text should not be merged into one sentence.
3. Explain why the trace-panel evidence suffixes improve trust in the exercise system.
4. Describe the full Arena Lab proof loop in one paragraph.

## Runtime Proof Checkpoint

At this point, you should be able to explain how one Arena Lab interaction becomes a full proof event: the learner acts, the scene advances, the exercise system marks progress, the trace panel updates, the prompt changes, the observation text reframes the result, and the event log records what happened.

## Recap

This lesson connected the Arena Lab to the proof layer:

- step 1 is marked directly from manual action
- later steps are derived from live scene state
- prompt and observation text guide both action and interpretation
- evidence suffixes and log lines turn progress into auditable proof

Next, we follow the Arena Lab render surface itself and show how its bars, checkpoints, depth meters, and HUD metrics make lifetime behavior visible on screen.
