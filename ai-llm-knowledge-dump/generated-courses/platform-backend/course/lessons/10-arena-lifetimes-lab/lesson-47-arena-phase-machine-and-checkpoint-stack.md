# Lesson 47: Arena Phase Machine and the Checkpoint Stack

## Frontmatter

- Module: `10-arena-lifetimes-lab`
- Lesson: `47`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `GAME_ARENA_CHECKPOINT_CAPACITY`
  - `arena_lab_set_checkpoint`
  - `arena_lab_apply_phase`
  - `phase_index`
  - `checkpoint_count`
  - `active_depth`
  - `simulated_temp_bytes`
  - `peak_temp_bytes`
  - `phase_name`
  - `phase_summary`

## Observable Outcome

By the end of this lesson, you should be able to retell the Arena Lab's eight-phase script and explain how each phase rebuilds the transient checkpoint story from scratch.

## Why This Lesson Exists

Lesson 46 explained what the Arena Lab scene stores.

That still leaves the most important question unanswered:

```text
how does the lab turn allocator lifetime ideas into a visible sequence
instead of just one static screen?
```

The answer is the phase machine in `arena_lab_apply_phase(...)`.

This function is the heart of the lab.

## First Reading Strategy

Read the phase machine as a script, not as a switch statement first.

1. list the eight named phases in plain language
2. separate grow, rewind, and handoff moments
3. only then match each one to the exact state fields it sets

That keeps the pedagogical story visible while you read the implementation.

## Step 0: Compare Two Neighboring Phases First

Before reading all eight phases, compare just these two:

```text
fan out work
rewind inner
```

The difference between them is the allocator lesson in miniature: inner speculative work can disappear instantly while the outer checkpoint still survives.

## Step 1: Read the Phase Machine as a Controlled Teaching Script

The lab does not try to simulate every possible scratch/temp pattern.

It chooses a smaller, clearer script.

Each phase corresponds to one recognizable allocator idea:

- baseline
- begin temp A
- nest temp B
- fan out work
- rewind inner
- refill branch
- rewind outer
- handoff

That is not arbitrary animation.

It is a deliberately ordered lifetime story.

## Visual: The Eight-Phase Story

```text
0 baseline
1 begin temp A
2 nest temp B
3 fan out work
4 rewind inner
5 refill branch
6 rewind outer
7 handoff
```

The lab repeats this cycle so the learner can step through it manually or watch it loop.

## Step 2: Read the Reset at the Top of `arena_lab_apply_phase(...)`

The function starts by clearing transient scene indicators:

```c
scene->checkpoint_count = 0;
scene->active_depth = 0;
scene->simulated_temp_bytes = 0;
```

This is a strong design choice.

It means every phase is rebuilt from explicit rules instead of mutating from the previous one in fragile incremental steps.

That keeps the demonstration deterministic.

## Why This Matters

If the function tried to update only the deltas from the previous phase, the scene would become harder to reason about and easier to break.

Instead the code says:

```text
start from a clean transient description
then declare exactly what this phase contains
```

That is a very good teaching pattern.

## Step 3: Read `arena_lab_set_checkpoint(...)` as a Visualization Helper

The helper writes one checkpoint slot:

- label
- `used_bytes`
- fill ratio
- color

It does not manage real arena memory.

It only populates the scene's checkpoint visualization state safely.

So the helper's real job is:

```text
turn one conceptual checkpoint into one visible checkpoint card
```

## Step 4: Walk the Middle Phases Carefully

The most important phases are the ones that make nesting and rewind visible.

### Phase 1: `begin temp A`

The lab sets:

- `checkpoint_count = 1`
- `active_depth = 1`
- `simulated_temp_bytes = 96`

and fills checkpoint `temp A`.

### Phase 2: `nest temp B`

Now the lab sets:

- `checkpoint_count = 2`
- `active_depth = 2`
- `simulated_temp_bytes = 208`

and keeps `temp A` while adding `temp B`.

### Phase 3: `fan out work`

Now the scene reaches:

- `checkpoint_count = 3`
- `active_depth = 3`
- `simulated_temp_bytes = 352`

with `temp C` added on top.

This is the maximum nesting depth of the scripted example.

## Step 5: Walk the Rewind Phases

### Phase 4: `rewind inner`

The scene drops back to two visible checkpoints:

- `checkpoint_count = 2`
- `active_depth = 2`
- `simulated_temp_bytes = 208`

This is the clearest proof that inner speculative work disappeared while outer work survived.

### Phase 5: `refill branch`

The scene reuses the same depth but with a different branch:

- still depth `2`
- now `temp B2`
- `simulated_temp_bytes = 248`

This is one of the strongest allocator lessons in the lab.

The code is teaching:

```text
after rewind, you can cheaply reuse the branch depth
without pretending the old speculative work still exists
```

### Phase 6: `rewind outer`

The scene returns to no active checkpoints.

That is the full unwind.

### Phase 7: `handoff`

The phase summary explicitly says:

```text
perm and level survive; scratch work is gone
```

That is the scene's final teaching sentence before the cycle loops.

## Step 6: Read `simulated_temp_bytes` as Deliberate Simulation, Not Real Allocator Accounting

This field is named carefully.

It is not claiming to be the literal current byte count of live temp scopes in `arena.h`.

It is a scene-side teaching metric that moves with the scripted checkpoint stack.

That matters because it lets the lab illustrate the idea cleanly without coupling every visual change to a live arena mutation.

## Step 7: Read `peak_temp_bytes` as Longitudinal Evidence

At the bottom of the function:

```c
if (scene->simulated_temp_bytes > scene->peak_temp_bytes)
  scene->peak_temp_bytes = scene->simulated_temp_bytes;
```

This gives the learner a second kind of metric:

```text
current transient pressure
vs
highest transient pressure reached so far
```

That is a nice example of “current state” versus “historical evidence.”

## Worked Example: Why `refill branch` Is Not Redundant

It would be easy to jump directly from `rewind inner` to `rewind outer`, but then the learner would only see "grow, then shrink."

`refill branch` teaches the stronger idea:

```text
after rewind, the same checkpoint depth can be reused for different transient work
```

That is why the scene bothers to show `temp B2` instead of ending the script early.

## Common Mistake: Thinking the Phase Machine Is Hiding the Real Mechanism

It is simplifying the mechanism, not falsifying it.

The allocator module already taught the actual `TempMemory` machinery.

The Arena Lab is doing something different:

```text
compress allocator lifetime behavior into a visible repeatable script
```

That is exactly what a good teaching scene should do.

## JS-to-C Bridge

This is like an explicit demo state machine for a complex subsystem. Instead of exposing every internal branch of the real implementation, it cycles through a curated sequence that makes the important invariants obvious.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find the reset at the top of `arena_lab_apply_phase(...)` and explain why it clears transient fields before switching on phase.
2. Find every phase name and summarize what it is trying to teach.
3. Compare phases 3, 4, and 5 and explain why that sequence is stronger than showing only a simple “grow then clear” cycle.
4. Find where `peak_temp_bytes` is updated and explain what kind of evidence it stores.
5. Explain why `simulated_temp_bytes` is named `simulated`.

## Exercises

1. Explain why the lab uses an eight-phase scripted loop instead of a fully free-form scratch allocator simulation.
2. Explain why `refill branch` is an important distinct phase instead of jumping directly from `rewind inner` to `rewind outer`.
3. Explain the difference between `checkpoint_count` and `active_depth` in this scene.
4. Describe the Arena phase machine in one short paragraph.

## Runtime Proof Checkpoint

At this point, you should be able to explain exactly how the Arena Lab makes nested checkpoints visible: each phase rebuilds the transient stack description from scratch, updates the visible depth and byte metrics, and leaves a stable textual explanation beside the scene.

## Recap

This lesson opened the Arena Lab's core phase script:

- the phase machine is a curated teaching sequence
- each phase rebuilds the transient visualization deterministically
- nested checkpoints, rewind, refill, and handoff each have their own explicit state
- current and peak temp pressure are tracked separately

Next, we follow the code that advances this phase machine, including the split between timed auto-advance and learner-driven manual stepping.
