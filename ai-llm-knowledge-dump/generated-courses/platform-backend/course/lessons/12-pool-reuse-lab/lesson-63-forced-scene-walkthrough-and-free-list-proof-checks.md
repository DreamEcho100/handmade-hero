# Lesson 63: Forced-Scene Walkthrough and Free-List Proof Checks

## Frontmatter

- Module: `12-pool-reuse-lab`
- Lesson: `63`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `--force-scene`
  - `--lock-scene`
  - `COURSE_FORCE_SCENE_INDEX`
  - `COURSE_LOCK_SCENE`
  - `GAME_SCENE_POOL_LAB`
  - `game_scene_exercise_prompt`
  - `game_scene_exercise_observation`
  - `game_scene_trace_step_evidence`
  - `pool_alloc`
  - `pool_free`

## Why This Lesson Exists

The previous five lessons explained the Pool Reuse Lab from the inside.

This lesson turns that explanation into a repeatable verification procedure.

The goal is not to admire the scene. The goal is to prove, inside a locked runtime, that one bounded pool can cycle from occupancy to return to real reuse without ever growing its backing store.

## Observable Outcome

By the end of this lesson, you should be able to run the Pool Reuse Lab in isolation and prove four claims with live evidence:

- fixed slots are allocated from one bounded pool
- collisions and exits return those slots to the free list
- returned slots reappear as available capacity before they are reused
- later allocations can reuse the same slot instead of needing more backing storage

## First Reading Strategy

Treat this lesson like an operator checklist, not like prose.

1. lock the runtime to the Pool scene
2. confirm the initial occupancy surfaces
3. inject deliberate churn with `SPACE`
4. wait for a return event before claiming reuse
5. only then confirm generation and reuse evidence

If you rush straight from burst to conclusion, you will miss the most important transition in the scene: returned capacity becoming visible before reuse occurs.

## Step 0: Use Isolation on Purpose

Use a forced-scene build so the runtime stays on one allocator story:

```bash
./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r
```

The exact value matters.

- scene index `2` is the Pool Reuse Lab
- `--lock-scene` keeps the runtime from drifting into a different lesson while you verify this one

This is the right setup because the Pool walkthrough depends on watching one allocator lifecycle without scene-switching noise.

## Step 1: Confirm the Initial Occupancy State

At boot, verify these baseline facts before pressing anything:

- the scene name is `Pool Reuse Lab`
- the summary mentions fixed-size reuse with a free-list pool
- the slot board already contains active cells because the scene pre-spawns probes
- the HUD line reports non-zero `active`
- step 1 may complete quickly because fixed-slot allocation is already visible
- the audio panel title reads `Audio Trace: voice churn`

This baseline matters because the scene intentionally starts from visible occupancy instead of from an empty board.

## Step 2: Pass 1, Read Stable Occupancy Before Forcing More Pressure

Do nothing for a few moments.

Verify:

- probes are already moving through the scene
- some probes show labels like `sN gM`
- the slot board agrees with current occupancy
- the HUD line and the board describe the same active population

This first pass ensures you can already read the proof surfaces before you add more churn.

## Step 3: Pass 2, Inject a Burst and Watch Pressure Spread Across Surfaces

Press `SPACE` once.

Verify:

- the event log records a pool burst
- `burst_count` increases
- more probes appear on screen
- collisions become more likely as the scene gets busier
- the warning line moves toward yellow or red
- the trace continues pointing toward churn and free-list return, not rebuild

This is the deliberate stress pass. You are not proving reuse yet. You are proving that the scene has entered a higher-pressure state that should eventually produce returns.

## Step 4: Pass 3, Wait for a Slot to Return

Now wait.

This is the step many learners skip, but it is critical.

Verify:

- `collision_count` increases as barriers get hit
- at least one probe expires or exits
- the trace panel completes the `return slot to the free list` step
- free capacity becomes visible through the trace evidence or board state
- the observation text keeps steering you toward returned capacity and later reuse

This is the transition from occupancy to availability. If you do not see this step clearly, the later reuse proof will be much weaker.

## Step 5: Pass 4, Confirm Reuse of a Returned Slot

After a return, wait for the next spawn cycle.

Verify:

- `reuse_hits` becomes non-zero
- the trace panel completes the final reuse step
- a slot that was free becomes active again on the board
- generation on the reused occupant is greater than `1`
- `hottest_slot` starts to matter if reuse concentrates repeatedly in one location

This is the actual allocator proof in the scene. The backing storage did not grow. One old slot became a new occupant.

## Visual: The Minimal Pool Proof Loop

```text
boot locked pool scene
  -> confirm occupied board
  -> press SPACE to add churn pressure
  -> wait for a slot return
  -> wait for a later spawn to reuse returned capacity
  -> confirm generation and reuse evidence
```

If you can explain that loop from memory, the module has landed.

## Step 6: Map the Walkthrough Back to the Allocator Code

By the end of the walkthrough, you should be able to connect each visible step back to one concrete allocator operation.

- slot becomes active -> `pool_alloc(...)`
- slot returns -> `pool_free(...)`
- immediate reuse -> the same pointer comes back out, which increments `reuse_hits`

The scene is valuable because it makes these low-level events visible without forcing the learner to stop and re-read code after every interesting frame.

## Worked Example: What Counts as a Strong Explanation

Weak explanation:

```text
I saw probes disappear and other probes show up.
```

Strong explanation:

```text
after the burst, collisions increased and one probe returned its slot to the free list;
later the board reactivated that same stable slot, the generation label rose above 1,
and the reuse counter proved the pool reused an old slot instead of growing storage
```

That is the standard the walkthrough is trying to build.

## Step 7: Notice Why Waiting Is Part of the Proof, Not Dead Time

The Pool walkthrough is intentionally not a rapid-fire button sequence.

The learner must see three distinct moments:

- pressure increase
- returned capacity
- later reuse

If those moments blur together, the free-list story becomes much harder to explain accurately.

That is why the walkthrough keeps telling you to wait after the burst and again after the return.

## Common Mistake: Treating the Walkthrough as Purely Visual

Visual motion is not enough.

The real verification standard is:

- can you explain which event proved returned capacity existed?
- can you explain which field or label proved reuse happened later?
- can you explain why the board never needed additional backing storage?

If those answers are clear, you are no longer just watching animation. You are reading allocator evidence.

## JS-to-C Bridge

This is like proving that a bounded worker pool is really reusing worker objects instead of secretly allocating more. Queue pressure is useful context, but the decisive evidence is that an old stable slot becomes active again with a later generation.

## Try Now

Use the forced-scene build, then do these checks:

1. Start with `./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r` and confirm you boot directly into the Pool Reuse Lab.
2. Explain the initial board state before pressing anything.
3. Press `SPACE` once and explain which surfaces prove churn pressure increased.
4. Wait for a slot return and explain which surfaces prove capacity came back to the pool.
5. Wait for reuse and explain which surfaces prove a returned slot became active again.

## Exercises

1. Explain why the Pool walkthrough requires waiting between actions instead of only pressing buttons quickly.
2. Explain why generation labels are critical for proving real reuse.
3. Explain why a bounded slot board is stronger evidence than only watching active probe count.
4. Describe the full Pool walkthrough as an ordered proof procedure.

## Runtime Proof Checkpoint

At this point, you should be able to run the Pool Reuse Lab in isolation and prove, with live evidence, the full free-list cycle from active occupancy to returned capacity to actual reuse.

## Recap

This lesson closed the Pool Reuse module:

- the scene can be isolated with forced-scene builds
- burst pressure can be injected deliberately with `SPACE`
- returned capacity becomes visible before reuse
- later allocations prove real free-list reuse instead of hidden growth
- the walkthrough maps directly back to `pool_alloc(...)` and `pool_free(...)`

Next, we move to a fourth allocator story: the Slab + Audio Lab, where pressure no longer comes from returning fixed slots but from same-size slab pages, transport-state changes, and audio-event crowding.
