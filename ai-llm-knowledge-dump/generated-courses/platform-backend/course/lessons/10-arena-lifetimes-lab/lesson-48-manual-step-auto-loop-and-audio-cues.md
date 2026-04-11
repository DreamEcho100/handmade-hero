# Lesson 48: Manual Step, Auto Loop, and Audio Cue Pressure

## Frontmatter

- Module: `10-arena-lifetimes-lab`
- Lesson: `48`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/game/base.h`
  - `src/game/audio_demo.c`
- Target symbols:
  - `game_advance_arena_lab`
  - `game_update_arena_lab`
  - `button_just_pressed`
  - `play_tone`
  - `manual_steps`
  - `rewind_count`
  - `phase_timer`
  - `beat`
  - `game_fire_scene_warning_probe`

## Observable Outcome

By the end of this lesson, you should be able to explain why the Arena Lab uses both passive auto-advance and learner-owned manual stepping, and why the manual path is intentionally allowed to win immediately in the frame where it happens.

## Why This Lesson Exists

Lesson 47 explained the phase machine as a state story.

This lesson explains the operator story.

The Arena Lab is not only trying to show nested temp scopes. It is also trying to teach them in a way that works for two different kinds of viewing:

- passive viewing, where the learner just watches the scene stay alive
- active testing, where the learner injects a deliberate step and studies the evidence

If the runtime supported only auto-advance, the scene would be readable but hard to interrogate. If it supported only manual stepping, the scene would be precise but often visually dead. The actual control flow is built to do both jobs.

## First Reading Strategy

Read the Arena Lab control flow in two layers:

1. `game_update_arena_lab(...)` decides whether this frame should advance the scene
2. `game_advance_arena_lab(...)` performs the actual phase transition and proof updates

That split is the architectural center of the lesson.

## Step 0: Start With One Fresh `SPACE` Press

Before you look at the whole function, compress the interaction model into one frame.

Imagine the learner presses `SPACE` during the baseline phase. Ask four questions in order:

1. why is this press treated as a learner-owned event instead of just another timer tick?
2. what state changes happen immediately?
3. what proof surfaces update because of that decision?
4. why would the lesson become weaker if the frame also auto-advanced a second time?

Those four questions are the whole teaching model in miniature.

## Step 1: Read `game_update_arena_lab(...)` as the Scene Scheduler

The update function is short:

```c
scene->beat += dt;
scene->phase_timer += dt;
if (button_just_pressed(input->buttons.play_tone)) {
  ... manual step path ...
  return;
}
if (scene->phase_timer >= 0.9f) {
  ... auto step path ...
}
```

That gives the scene two legitimate advancement modes:

```text
manual advance on a fresh SPACE press
automatic advance every 0.9 seconds otherwise
```

This is not redundancy. These two paths exist for different reasons.

- auto-advance keeps the scene informative even if nobody presses anything
- manual stepping creates a clean cause-and-effect moment the learner can own

The Arena Lab is therefore both a demo loop and an experiment surface.

## Step 2: Understand Why Manual Input Wins the Frame

On a fresh `play_tone` press, the code does this work in order:

- resets `phase_timer`
- marks exercise step 1
- calls `game_advance_arena_lab(..., manual_step = 1)`
- conditionally fires a warning probe when depth is already crowded enough
- returns immediately

That early return is not just a safety detail. It is a teaching choice.

If the manual path fell through into the timed path, one press could produce two different advancement causes in the same frame:

- the learner's deliberate button press
- the scene's passive timer threshold

That would blur the proof. The learner would no longer be able to say, with confidence:

```text
this phase change happened because I pressed SPACE right now
```

The return keeps the event single-cause and auditable.

## Visual: Why the Early Return Matters

```text
manual frame
  beat += dt
  phase_timer += dt
  SPACE just pressed
    -> reset phase_timer
    -> mark exercise step 1
    -> advance exactly once
    -> maybe fire warning probe
    -> stop

auto frame
  beat += dt
  phase_timer += dt
  no fresh SPACE
  timer >= 0.9
    -> advance exactly once
```

Both paths advance once. The difference is who owns the explanation for that advance.

## Step 3: Read `game_advance_arena_lab(...)` as the Official Transition Function

`game_update_arena_lab(...)` chooses whether an advance should happen now.

`game_advance_arena_lab(...)` defines what an advance means.

That second function performs the real state transition:

- compute and wrap the next phase index
- increment `cycle_count` when the phase loop wraps
- increment `manual_steps` only for learner-owned advances
- increment `rewind_count` when the scene lands on rewind phases
- rebuild the on-screen checkpoint story via `arena_lab_apply_phase(...)`
- log the new phase
- emit a tone that matches the new pressure state

That split keeps policy separate from mutation:

```text
update function -> when to advance
advance function -> what the new phase means
```

This is cleaner for code organization, but more importantly it makes the lesson readable. The learner can study timing rules separately from phase semantics.

## Step 4: Treat the Counters as Proof, Not Decoration

Inside `game_advance_arena_lab(...)`:

```c
next_phase = (scene->phase_index + 1u) % 8u;
if (next_phase == 0)
  scene->cycle_count++;
...
if (manual_step)
  scene->manual_steps++;
if (scene->phase_index == 4u || scene->phase_index == 6u)
  scene->rewind_count++;
```

Each counter proves a different claim:

- `cycle_count` proves the loop can keep teaching itself passively
- `manual_steps` proves the learner explicitly took control
- `rewind_count` proves the scene really visited rewind phases instead of only talking about them

The important detail is that `manual_steps` lives in the official transition function, not in a detached input counter. That means the runtime only records manual intent when it actually caused a phase transition.

## Step 5: See the Pedagogical Split Between Auto Loop and Manual Step

The Arena Lab needs both of these sentences to be true at the same time:

```text
I can watch the story continue on its own.
I can also stop guessing and inject one deliberate proof event.
```

Auto-advance serves the first sentence.

Manual stepping serves the second.

That is why the scene does not treat `SPACE` as a faster version of the timer. It treats `SPACE` as learner agency. The runtime marks exercise progress, counts the manual intervention separately, and preserves a clean one-press-one-transition rule so the learner can inspect the resulting HUD, trace, and audio evidence without ambiguity.

## Step 6: Read the Tone Logic as a Second Explanation Channel

After logging the phase, `game_advance_arena_lab(...)` emits one cue:

- low tone for rewind phases
- high tone for depth `>= 3`
- mid tone otherwise

That means the Arena Lab does not rely on visuals alone. It also sonifies checkpoint pressure.

## Visual: Phase Change to Cue Mapping

```text
rewind phase     -> low cue
deep nesting     -> high cue
ordinary advance -> mid cue
```

This matters pedagogically because the learner can hear the pressure story even before parsing the HUD.

## Step 7: Understand Why the Warning Probe Is Separate From the Basic Cue

The manual path may also call:

```c
game_fire_scene_warning_probe(game, GAME_SCENE_ARENA_LAB);
```

but only after the phase transition and only when `active_depth >= 2`.

That probe is not the same as the ordinary transition tone.

The ordinary tone answers:

```text
what kind of phase did we just enter?
```

The warning probe answers:

```text
did this learner-owned action push the scene into enough depth to warm the
warning surfaces too?
```

So the runtime layers its feedback:

- one cue for the phase transition itself
- an extra proof signal when the learner drove the scene into crowding pressure

## Step 8: Keep `beat` Separate From `phase_timer`

Two time fields advance every update:

- `beat` drives ambient motion such as packet drift
- `phase_timer` drives the passive advancement cadence

That split prevents a common design mistake: tying the animation loop and the teaching cadence to the same clock. The scene can keep moving smoothly even when the phase schedule is being reset, interrupted, or manually overridden.

## Worked Example: One Press, One Teachable Moment

Suppose `phase_timer` is already near `0.9f` and the learner presses `SPACE` on that frame.

Without the current design, the frame could become ambiguous:

- did the scene advance because of the timer?
- because of the press?
- or because both happened together?

With the current design, the answer is clean:

- the press wins
- the timer is reset
- the phase advances once
- the resulting log, cue, and counters belong to that learner-owned event

That is exactly the right control rule for a lab scene.

## Common Mistake: Thinking Auto-Advance Makes Manual Stepping Optional Noise

It does not.

Auto-advance makes the scene watchable.

Manual stepping makes the scene testable.

Those are different teaching jobs, and the lesson is stronger because the runtime supports both.

## JS-to-C Bridge

This is like a demo that auto-plays while still exposing a precise step button for debugging. Auto-play keeps the system legible at a glance. The step button turns the same scene into a controlled experiment where the learner can create exactly one event and inspect the evidence it produced.

## Try Now

Open `src/game/main.c` and `src/game/base.h`, then do these checks:

1. Find the `button_just_pressed(input->buttons.play_tone)` path and explain why it returns early.
2. Find where `manual_steps` is incremented and explain why it lives in the transition function instead of a detached input counter.
3. Find where `rewind_count` is incremented and explain why those phases deserve their own proof counter.
4. Find the phase-cue audio logic and explain the difference between low, mid, and high cues.
5. Find where `game_fire_scene_warning_probe(...)` is called and explain why it is gated on `active_depth >= 2`.

## Exercises

1. Explain why `game_update_arena_lab(...)` and `game_advance_arena_lab(...)` are separated instead of merged into one function.
2. Explain why the Arena Lab needs both passive auto-advance and learner-owned manual stepping.
3. Explain the difference between an ordinary phase cue and a warning probe sequence.
4. Describe the Arena Lab interaction model in one paragraph, including why the early return matters.

## Runtime Proof Checkpoint

At this point, you should be able to explain exactly how the learner takes control of the Arena Lab: one fresh SPACE press forces an immediate manual step, logs the event, may heat the warning path, and then yields back to a slower automatic loop.

## Recap

This lesson opened the Arena Lab's control flow:

- the scene auto-advances on a timer but yields to manual stepping
- one central advance function owns the official phase transition
- counters and cue sounds turn phase changes into proof signals
- warning probes are an extra pressure channel, not the same thing as normal phase cues

Next, we connect this scene motion to the proof layer: exercise marks, prompt text, observation text, and event-log evidence.
