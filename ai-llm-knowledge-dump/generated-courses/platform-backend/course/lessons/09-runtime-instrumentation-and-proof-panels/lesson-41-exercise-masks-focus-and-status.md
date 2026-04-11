# Lesson 41: Exercise Masks, Focus, and Derived Status

## Frontmatter

- Module: `09-runtime-instrumentation-and-proof-panels`
- Lesson: `41`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `GameSceneExerciseState`
  - `GAME_EXERCISE_STEP_1`
  - `GAME_EXERCISE_STEP_2`
  - `GAME_EXERCISE_STEP_3`
  - `GAME_EXERCISE_STEP_4`
  - `game_scene_exercise_focus_step`
  - `game_scene_exercise_required_mask`
  - `game_scene_exercise_status_name`
  - `game_mark_scene_exercise`

## Observable Outcome

By the end of this lesson, you should be able to explain how the runtime stores proof progress compactly, how it derives the next focus step from that compact state, and why status text is computed from masks instead of being stored as a separate mutable UI string.

## Prerequisite Bridge

Module 08 finished the scene machine.

That means the runtime can now:

- choose a scene
- rebuild scene-owned state
- coordinate scene-enter side effects

Module 09 adds a different layer beside that machinery:

```text
how does the runtime remember what the learner has actually proved?
```

The first answer is the exercise-mask model.

## Why This Lesson Exists

If the course only drew text labels like "done" or "not done," the proof system would be fragile.

Instead, this runtime stores a real progress state model and derives HUD behavior from it.

That is why this lesson starts with bits and masks, not with drawing code.

## Step 1: Start With the Step-Bit Definitions

Near the top of `src/game/main.c`:

```c
#define GAME_EXERCISE_STEP_1 (1u << 0)
#define GAME_EXERCISE_STEP_2 (1u << 1)
#define GAME_EXERCISE_STEP_3 (1u << 2)
#define GAME_EXERCISE_STEP_4 (1u << 3)
```

Each exercise step owns one bit.

That gives the runtime a compact representation of multi-step proof progress.

## Visual: One Four-Step Mask

```text
bit 0 -> step 1
bit 1 -> step 2
bit 2 -> step 3
bit 3 -> step 4

example:
progress_mask = 0101b
means:
  step 1 complete
  step 2 incomplete
  step 3 complete
  step 4 incomplete
```

That is the core state model of the exercise system.

## Step 2: Read `GameSceneExerciseState`

The per-scene proof struct is:

```c
typedef struct {
  unsigned int progress_mask;
  unsigned long long completed_frame;
} GameSceneExerciseState;
```

This answers two important questions.

### `progress_mask`

Which proof steps are complete?

### `completed_frame`

If all required steps are complete, when did the runtime first record that full completion?

That is enough to drive step labels, focus, status, and completion side effects.

## Step 3: Read `game_scene_exercise_required_mask(...)`

The helper returns the set of steps that matter for a given scene.

On this branch, every active scene currently requires all four steps:

```c
return GAME_EXERCISE_STEP_1 | GAME_EXERCISE_STEP_2 |
       GAME_EXERCISE_STEP_3 | GAME_EXERCISE_STEP_4;
```

Even so, the helper still matters.

It keeps the rule centralized instead of baking it into multiple HUD branches.

That means the runtime can later change per-scene requirements without redesigning the whole proof system.

## Step 4: Read `game_scene_exercise_focus_step(...)` as "Find the First Missing Proof"

The helper walks the required mask in step order and returns the first incomplete required step index.

Its logic is effectively:

```text
if step 1 is required and incomplete -> focus step 1
else if step 2 is required and incomplete -> focus step 2
else if step 3 is required and incomplete -> focus step 3
else -> focus the last slot
```

This is how the HUD knows what the learner should pay attention to next.

## Worked Example: Focus from a Partial Mask

Suppose:

```text
required_mask = 1111b
progress_mask = 0101b
```

That means:

- step 1 done
- step 2 missing
- step 3 done
- step 4 missing

The focus helper returns step index `1`, meaning:

```text
highlight step 2 next
```

That is why the trace panel can be guided instead of passive.

## Step 5: Read `game_scene_exercise_status_name(...)` as Derived UI State

The helper returns:

- `ready`
- `in progress`
- `complete`

based on the mask state.

This is a strong rule:

```text
derive display state from real proof data whenever possible
```

That keeps the HUD honest.

If status were stored as a separate mutable string, it could drift from the real mask state.

## Step 6: Read `game_mark_scene_exercise(...)` as the Official Proof Commit Path

When a scene proves a step, the runtime calls:

```c
exercise->progress_mask |= step_mask;
```

Then it may log an event, and if the full required mask is now satisfied, it records:

```c
exercise->completed_frame = game->frame_index;
```

and triggers a completion sound.

That means this helper is more than a bit setter.

It is the official place where proof becomes real to the rest of the runtime.

## Step 7: Notice That Completion Lives Beside Logs and Audio Feedback

The helper does not only update the mask.

It also coordinates:

- HUD log output
- completion-time bookkeeping
- reward audio

That is exactly why Module 09 is not just a UI module. It is a proof-state module.

## Visual: One Step Becoming Officially Complete

```text
scene behavior proves a step
  -> game_mark_scene_exercise(...)
  -> bit is set in progress_mask
  -> log may be appended
  -> completion frame may be recorded
  -> focus/status helpers instantly see the new state
```

That is the real state flow.

## Step 8: Why a Bitmask Fits This Runtime Well

This branch has four exercise steps per scene.

For that scale, a bitmask is a very good fit because it is:

- compact
- easy to combine with `required_mask`
- easy to query for completion or incompletion
- easy to count later with helpers like `game_count_bits(...)`

This is a small but solid data-model decision.

## Practical Exercises

### Exercise 1: Explain `required_mask`

Why does the runtime have a separate `required_mask` if it already has `progress_mask`?

Expected answer:

```text
because progress says what is done, while required_mask says which steps actually matter for the current scene's proof model
```

### Exercise 2: Explain Focus

Why is `game_scene_exercise_focus_step(...)` more useful than only showing how many steps are complete?

Expected answer:

```text
because it tells the HUD which specific incomplete proof the learner should focus on next instead of only summarizing past progress
```

### Exercise 3: Explain Derived Status

Why is it good that `game_scene_exercise_status_name(...)` derives its answer from masks instead of storing status text separately?

Expected answer:

```text
because derived status stays consistent with the underlying proof bits and avoids the UI drifting away from real runtime state
```

## Common Mistakes

### Mistake 1: Thinking the proof system is only text labels

It begins with a real state model.

### Mistake 2: Treating `completed_frame` as redundant

It preserves completion timing, which the HUD and event system can use as separate evidence from just "all bits are set now."

### Mistake 3: Forgetting that masks support both progress and selection

The same compact model drives completion checks, focus selection, and status derivation.

## Troubleshooting Your Understanding

### "Why not store four booleans instead?"

You could, but the mask model makes combination with required-step logic cleaner and more compact.

### "Why does the runtime need an official mark helper instead of setting bits inline everywhere?"

Because marking a step also drives logs, completion-time recording, and reward audio, so the proof transition belongs in one controlled function.

## Recap

You now understand the proof-state core:

1. step bits encode completion compactly
2. `GameSceneExerciseState` stores progress plus completion time
3. `required_mask` defines which steps matter
4. focus and status are derived from masks
5. `game_mark_scene_exercise(...)` is the official proof-commit path

## Next Lesson

Lesson 42 shows how the runtime turns that compact proof state into dynamic guidance: one line telling the learner what to do next and a second line telling them what evidence to observe after they do it.
