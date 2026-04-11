# Lesson 13: `GameButtonState`, Edge Detection, and Per-Frame Button Truth

## Frontmatter

- Module: `04-input-timing-and-presentation`
- Lesson: `13`
- Status: Required
- Target files:
  - `src/game/base.h`
- Target symbols:
  - `GameButtonState`
  - `button_just_pressed`
  - `button_just_released`
  - `UPDATE_BUTTON`

## Observable Outcome

By the end of this lesson, you should be able to look at one `GameButtonState` value and tell a plausible story for what happened to that button during the current frame.

You should be able to explain why the runtime stores two facts instead of one boolean:

- where the button ended the frame
- how many state changes happened during the frame

## Prerequisite Bridge

Lesson 12 established that the runtime needs explicit truth rules.

The next truth the course defines is not a pointer invariant or a resize policy.

It is this:

```text
what happened to this button during this frame?
```

That question is richer than “is the key down right now?” and games care about the richer version.

## Why This Lesson Exists

Games need both sustained state and edges.

They care about questions like:

- is the key still held?
- was it pressed this frame?
- was it released this frame?
- did it flip twice inside one long frame?

One boolean cannot answer all of those questions.

So the course introduces a small but powerful per-button state model.

## Step 1: Read the Live Type and Helpers Together

In `src/game/base.h`:

```c
typedef struct {
  int ended_down;
  int half_transitions;
} GameButtonState;

static inline int button_just_pressed(GameButtonState button) {
  return button.ended_down && (button.half_transitions > 0);
}

static inline int button_just_released(GameButtonState button) {
  return !button.ended_down && (button.half_transitions > 0);
}
```

This is the atomic unit of the input system.

Everything in Lesson 14 builds on this one small struct.

## Step 2: Understand the Two Stored Facts

### `ended_down`

This answers:

```text
what physical state did the button finish in at the end of the frame?
```

### `half_transitions`

This answers:

```text
how many times did the button flip during the frame?
```

So the model is really a two-part frame report:

```text
where did I end?
how much did I change on the way there?
```

## Step 3: Why the Final Boolean Alone Is Not Enough

Suppose a button starts up, gets pressed, and then released again all inside one frame:

```text
up -> down -> up
```

The final physical state is still up.

So if the runtime stored only one boolean, it would completely miss the fact that anything happened.

But with the course's model, that same frame becomes:

```text
ended_down = 0
half_transitions = 2
```

Now the frame history is preserved.

## Worked Table: Common Button Stories

| Story                                   | `ended_down` | `half_transitions` |
| --------------------------------------- | -----------: | -----------------: |
| held from previous frame, unchanged now |            1 |                  0 |
| freshly pressed this frame              |            1 |                  1 |
| freshly released this frame             |            0 |                  1 |
| tapped fully inside one frame           |            0 |                  2 |

This table is worth memorizing because it makes the model concrete very quickly.

## Step 4: Read the Helper Functions as Questions

`button_just_pressed(button)` means:

```text
did the button finish this frame down,
and did it change at least once during this frame?
```

`button_just_released(button)` means:

```text
did the button finish this frame up,
and did it change at least once during this frame?
```

These helpers do not store new information.

They simply ask readable questions over the two stored facts.

## Step 5: `UPDATE_BUTTON` Is the Transition Gate

The macro is:

```c
#define UPDATE_BUTTON(btn_ptr, is_down_val)                                    \
  do {                                                                         \
    GameButtonState *_btn = (btn_ptr);                                         \
    if (_btn->ended_down != (is_down_val)) {                                   \
      _btn->ended_down = (is_down_val);                                        \
      _btn->half_transitions++;                                                \
    }                                                                          \
  } while (0)
```

This enforces the core rule of the button model:

```text
only record a transition when the physical state actually changed
```

That sounds obvious, but it is the whole reason held inputs stay stable.

## Visual: What the Conditional Prevents

```text
old state == new state
  -> no transition

old state != new state
  -> update ended_down
  -> increment half_transitions
```

Without that conditional, held keys would accumulate fake transitions every frame and edge detection would become meaningless.

## Step 6: Walk Through Three Timelines

### Story 1: Press this frame

```text
start of frame: ended_down = 0
event: press
end of frame:   ended_down = 1, half_transitions = 1
```

### Story 2: Release this frame

```text
start of frame: ended_down = 1
event: release
end of frame:   ended_down = 0, half_transitions = 1
```

### Story 3: Tap fully inside one frame

```text
start of frame: ended_down = 0
events: press, release
end of frame:   ended_down = 0, half_transitions = 2
```

That third case is the best proof that one final boolean is not enough.

## Step 7: Why the Name `half_transitions` Exists

The term can sound odd at first.

It is counting state flips, not full press cycles.

So:

- up to down = one half-transition
- down to up = one half-transition

A full tap inside one frame is therefore two half-transitions.

That naming matches what the runtime actually measures.

## Step 8: Separate Held State From Edge State

This model supports both kinds of gameplay logic:

### Held-state questions

```text
is the button still down?
```

Use `ended_down`.

### Edge questions

```text
did the button become active this frame?
did it stop being active this frame?
```

Use `button_just_pressed(...)` and `button_just_released(...)`.

That separation is what makes the control system reliable.

## Worked Example: Why `ended_down == 1` Is Not “Just Pressed”

Suppose the space bar has been held for five consecutive frames.

On the current frame:

```text
ended_down = 1
half_transitions = 0
```

So:

```text
button_just_pressed(...) -> false
```

That is correct.

The key is held, but it was not newly pressed this frame.

This single example is enough to show why “is down” and “became down this frame” must stay distinct.

## Practical Exercises

### Exercise 1: Decode the State

Interpret each state below in plain language:

1. `ended_down = 1`, `half_transitions = 0`
2. `ended_down = 1`, `half_transitions = 1`
3. `ended_down = 0`, `half_transitions = 2`

Expected answers:

```text
1 -> held from before, unchanged this frame
2 -> pressed this frame and ended held
3 -> tapped fully inside the frame
```

### Exercise 2: Predict the Helper Results

For a button state with:

```text
ended_down = 0
half_transitions = 1
```

What do the helpers return?

Expected result:

```text
button_just_pressed  -> false
button_just_released -> true
```

### Exercise 3: Explain the Conditional in `UPDATE_BUTTON`

Why does the macro update only when `_btn->ended_down != is_down_val`?

Expected answer:

```text
so held keys do not generate fake transitions on frames where nothing changed
```

## Common Mistakes

### Mistake 1: Treating `ended_down` as the whole story

It only tells you where the button ended, not what happened during the frame.

### Mistake 2: Treating `half_transitions` as a weird corner-case field

It is what makes normal “just pressed” and “just released” behavior possible.

### Mistake 3: Thinking the helpers are new stored state

They are just readable queries over the existing two facts.

## Troubleshooting Your Understanding

### “I still think a single boolean should be enough”

Re-run the tap-inside-one-frame example:

```text
up -> down -> up
```

If you store only the final state, you lose the entire event.

### “Why does the course care about multi-transition frames at all?”

Because frame boundaries are coarse compared to real input timing.

The runtime needs a model that stays meaningful even when more than one event lands inside the same frame.

## Recap

You now have the runtime's per-button truth model:

1. `ended_down` stores the final physical state for the frame
2. `half_transitions` stores how many flips happened during the frame
3. helpers like `button_just_pressed(...)` are readable queries over those two facts
4. `UPDATE_BUTTON` increments transitions only when the state actually changes

## Next Lesson

Lesson 13 defined one button across one frame.

Lesson 14 scales that up to the whole input snapshot by explaining how the runtime reuses two `GameInput` buffers to preserve previous-frame truth while constructing the next one.
