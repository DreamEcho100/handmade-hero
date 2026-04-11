# Lesson 14: Double-Buffered Input Snapshots and Frame Assembly

## Frontmatter

- Module: `04-input-timing-and-presentation`
- Lesson: `14`
- Status: Required
- Target files:
  - `src/game/base.h`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `GameInput`
  - `prepare_input_frame`
  - `platform_swap_input_buffers`
  - `process_events`

## Observable Outcome

By the end of this lesson, you should be able to explain how the runtime constructs one stable input snapshot per frame without allocating a new input object every frame.

You should be able to state the model precisely:

```text
one GameInput holds the last completed frame
one GameInput is reused to assemble the current frame
at the frame boundary, the two roles swap
```

## Prerequisite Bridge

Lesson 13 explained one button across one frame.

Now the course needs to answer the larger runtime question:

```text
how do we preserve last frame's finished button truths
while building a clean snapshot for this frame?
```

The answer is double buffering.

## Why This Lesson Exists

The game never consumes only one button.

It consumes one whole-frame input report.

So the runtime needs a pattern that does all of the following at once:

- keep the previous frame available for reference
- build the current frame without corrupting the previous one
- avoid heap allocation every frame

The two-buffer pattern solves all three.

## Step 1: Read `GameInput` as One Frame Report

In `src/game/base.h`, `GameInput` groups many buttons into one snapshot:

```c
typedef struct {
  union {
    GameButtonState all[16];
    struct {
      GameButtonState quit;
      GameButtonState play_tone;
      GameButtonState cycle_scale_mode;
      ...
    };
  } buttons;
} GameInput;
```

The important perspective is this:

```text
GameInput is not "the keyboard forever"
GameInput is one frame's input report
```

The union gives the runtime two useful views:

- named access for gameplay code
- indexed access for loops that operate over every button

## Step 2: See the Two Actual Buffers in the Backends

Both backends allocate exactly two input snapshots:

```c
GameInput inputs[2];
memset(inputs, 0, sizeof(inputs));
GameInput *curr_input = &inputs[0];
GameInput *prev_input = &inputs[1];
```

That means:

```text
the runtime does not allocate fresh input objects every frame
it reuses two fixed snapshots and swaps their jobs
```

## Visual: Snapshot Roles Before a New Frame

```text
prev_input -> last completed frame report
curr_input -> reusable buffer that will become this frame's report
```

That is the starting state before new events are applied.

## Step 3: Why the Swap Happens Before Preparation

At the top of each frame, both backends do:

```c
platform_swap_input_buffers(&curr_input, &prev_input);
prepare_input_frame(curr_input, prev_input);
```

That order matters.

After the swap:

- `prev_input` points at the snapshot that just finished last frame
- `curr_input` points at the reusable buffer that will become the new snapshot

Only after those roles are correct does the runtime seed the new frame.

## Step 4: `platform_swap_input_buffers(...)` Is Pure Role Exchange

The helper is:

```c
static inline void platform_swap_input_buffers(GameInput **curr,
                                               GameInput **prev) {
  GameInput *tmp = *curr;
  *curr = *prev;
  *prev = tmp;
}
```

Notice what it does not do:

- no memory allocation
- no deep copy
- no platform API call

It only exchanges which buffer plays which role.

That is why it lives in `game/base.h` instead of `platform.h`.

This is shared input-model logic, not backend-specific behavior.

## Step 5: `prepare_input_frame(...)` Defines the New-Frame Reset Policy

The helper is:

```c
static inline void prepare_input_frame(GameInput *curr, const GameInput *prev) {
  int button_count =
      (int)(sizeof(curr->buttons.all) / sizeof(curr->buttons.all[0]));
  for (int i = 0; i < button_count; i++) {
    curr->buttons.all[i].ended_down = prev->buttons.all[i].ended_down;
    curr->buttons.all[i].half_transitions = 0;
  }
}
```

This one loop does two critical things:

1. carry forward whether each button is still held
2. clear transition counts so the new frame starts with fresh edge history

## Visual: What the Preparation Step Really Means

```text
copy held-state forward
reset per-frame transition counts to zero
let this frame record new changes cleanly
```

That is the exact policy the runtime wants.

## Worked Example: Holding Space Across Frames

Suppose Space ended frame `N` as:

```text
ended_down = 1
half_transitions = 1
```

At the start of frame `N + 1`:

1. the buffers swap roles
2. `prepare_input_frame(...)` copies `ended_down = 1` into the new `curr_input`
3. `half_transitions` resets to `0`

If Space remains held for the whole new frame, the final state becomes:

```text
ended_down = 1
half_transitions = 0
```

That correctly means:

```text
still held, not newly pressed this frame
```

This example is the payoff of the whole pattern.

## Step 6: Raylib and X11 Differ Only in Event Collection

Once the current snapshot is prepared, the backends differ only in how they fill it.

### Raylib path

Raylib polls host key state directly:

```c
UPDATE_BUTTON(&curr_input->buttons.play_tone, IsKeyDown(KEY_SPACE) ? 1 : 0);
UPDATE_BUTTON(&curr_input->buttons.cycle_scale_mode,
              IsKeyDown(KEY_TAB) ? 1 : 0);
```

### X11 path

X11 drains the event queue in `process_events(...)` and maps concrete key events into the same button model:

```c
case XK_space:
  UPDATE_BUTTON(&curr->buttons.play_tone, 1);
  break;

case XK_Tab:
  UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 1);
  break;
```

The APIs differ, but the shared architecture does not:

```text
different host input sources
same GameInput snapshot shape
same GameButtonState meaning
same UPDATE_BUTTON transition logic
```

## Step 7: Why Two Buffers Are Better Than One Forever-Mutating Object

If the runtime mutated a single global input object forever, it would blur two very different questions:

- what was true last frame?
- what changed during this frame?

Double buffering prevents that blur.

It keeps “last completed truth” and “current assembly state” as separate roles.

That separation is what makes edge detection from Lesson 13 interpretable.

## Worked Timeline: One Full Frame Boundary

```text
end of frame N:
  prev_input = older snapshot
  curr_input = finished snapshot for frame N

start of frame N+1:
  swap roles
  curr_input now points to reusable buffer
  prev_input now points to frame N snapshot

prepare_input_frame:
  copy ended_down from prev_input to curr_input
  reset half_transitions in curr_input

process events:
  write new changes into curr_input

game update:
  consume curr_input as the official frame N+1 snapshot
```

That is the full lifecycle in one timeline.

## Practical Exercises

### Exercise 1: Explain the Role Split

Without looking back, define `prev_input` and `curr_input` precisely.

Target answer:

```text
prev_input is the last completed frame snapshot;
curr_input is the snapshot currently being assembled for this frame
```

### Exercise 2: Predict the Prepared State

Suppose a button in `prev_input` is:

```text
ended_down = 1
half_transitions = 2
```

Immediately after `prepare_input_frame(curr, prev)`, what should that same button be in `curr`?

Expected result:

```text
ended_down = 1
half_transitions = 0
```

### Exercise 3: State the Shared Architecture

What part of the input system differs between Raylib and X11, and what part stays the same?

Expected answer:

```text
the host event collection differs;
the snapshot structure, button meaning, and update rules stay the same
```

## Common Mistakes

### Mistake 1: Thinking the two buffers are just “current keyboard” and “old keyboard”

That is too vague.

The sharper model is “last completed frame report” versus “this frame's report under construction.”

### Mistake 2: Forgetting why `half_transitions` resets every frame

Because transitions are frame-local history, not long-lived state.

### Mistake 3: Thinking platform-specific polling changes the shared meaning of `GameInput`

It changes only how the snapshot gets filled, not what the snapshot represents.

## Troubleshooting Your Understanding

### “Why swap before prepare?”

Because `prepare_input_frame(...)` needs the just-finished frame as the source of held-state truth.

The pointers have to be in the correct roles first.

### “Why not just copy the whole old snapshot and overwrite fields later?”

Because the runtime specifically wants to preserve only the held-state facts and reset the per-frame transition counts.

`prepare_input_frame(...)` encodes that policy directly.

## Recap

You now have the runtime's whole-frame input assembly model:

1. there are exactly two reusable `GameInput` snapshots
2. one represents the previous completed frame
3. one is reused to assemble the current frame
4. `prepare_input_frame(...)` copies held state forward and resets edge counts
5. Raylib and X11 differ only in how they feed host input into the shared snapshot model

## Next Lesson

Input now has a stable per-frame story.

Lesson 15 applies the same discipline to time by showing how one frame gets a budget, how the backends measure work, and how they pace toward `TARGET_FPS`.
