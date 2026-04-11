# Module 04: Input, Timing, and Presentation

Module 03 taught you how one frame image is built.

Module 04 teaches how one frame becomes trustworthy.

That shift matters.

At the end of the pixel-pipeline module, the runtime can build and present an image. But an interactive runtime needs more than a visible image. It also needs rules for:

- deciding what input became true this frame
- pacing work against a target frame budget
- reacting to resize events without losing presentation correctness

That is what this module installs.

---

## Why This Module Exists

Without this module, later scene labs would still run, but they would feel unreliable:

- input would look like ad hoc key polling
- timing would feel like vague "the engine runs every frame somehow"
- resize behavior would feel like backend trivia instead of explicit presentation policy

Module 04 fixes that by making the frame loop's control systems visible and teachable.

---

## Observable Outcome

By the end of Module 04, you should be able to:

1. Explain why debug assertions are for impossible internal states rather than recoverable failures.
2. Model one button as a two-fact frame snapshot instead of a raw key query.
3. Explain how two `GameInput` buffers are reused to assemble stable per-frame input.
4. Explain the difference between Raylib timing delegation and X11 timing ownership.
5. Compute letterbox scale and offsets by hand.
6. Explain when resize changes only presentation placement and when it changes the backbuffer itself.

If those ideas feel stable, the runtime has stopped being "just rendering" and has become a frame-based interactive program.

---

## Module Map

```text
Lesson 12
  debug traps and assertions

Lesson 13
  one button across one frame

Lesson 14
  two snapshots across two frames

Lesson 15
  one frame budget and backend pacing

Lesson 16
  aspect-fit and letterbox math

Lesson 17
  resize policy and scale modes
```

---

## Lesson Order

1. [lesson-12-debug-trap-and-assertions.md](lesson-12-debug-trap-and-assertions.md)
   - establish the fail-fast rule before more frame state arrives
2. [lesson-13-gamebuttonstate-and-edge-detection.md](lesson-13-gamebuttonstate-and-edge-detection.md)
   - model one button as end-state plus transition count
3. [lesson-14-double-buffered-input-snapshots.md](lesson-14-double-buffered-input-snapshots.md)
   - build whole-frame input snapshots by reusing two buffers
4. [lesson-15-frame-timing-and-the-60-fps-budget.md](lesson-15-frame-timing-and-the-60-fps-budget.md)
   - account for work time, total frame time, and pacing strategy
5. [lesson-16-letterbox-and-aspect-fit-math.md](lesson-16-letterbox-and-aspect-fit-math.md)
   - preserve aspect while fitting a canvas into a real window
6. [lesson-17-window-scale-modes-and-resize-handling.md](lesson-17-window-scale-modes-and-resize-handling.md)
   - turn resize behavior into explicit runtime policy

---

## The Big Continuity Bridge From Module 03

Module 03 ended with this idea:

```text
the runtime can build a correct image
```

Module 04 adds the next one:

```text
the runtime can build the correct image
at the correct time
from the correct frame snapshot
with the correct presentation policy
```

That is why these lessons belong immediately after the pixel pipeline instead of much later.

---

## How To Study This Module

For each lesson, keep asking two questions:

```text
what state persists across frames?
what state resets every frame?
```

That question is the hidden thread connecting the entire module.

It shows up in:

- assertions about valid persistent state
- per-frame button transitions
- reused input snapshots
- timing checkpoints
- scale-mode changes after window events

If you keep that question active, Module 04 becomes much easier to follow.

---

## Core Habits To Build Here

### Habit 1: Separate recoverable failures from invariant failures

Some problems should return errors. Some should break immediately in a debug build. The course starts making that distinction explicit here.

### Habit 2: Separate final state from per-frame change

That distinction is the basis of reliable button edges.

### Habit 3: Separate work time from total frame time

That distinction is the basis of honest frame pacing.

### Habit 4: Separate canvas policy from presentation math

Letterboxing and scale modes are related, but they are not the same layer.

---

## Beginner Danger Zones

Watch for these mistakes while reading Module 04:

1. Treating assertions as normal error handling.
2. Treating input as "whatever the key is right now" instead of a frame snapshot.
3. Thinking `dt`, FPS display, and frame budget are all the same concept.
4. Confusing "where should the image be drawn" with "what size should the backbuffer itself be."

This module exists largely to prevent those four mistakes from infecting everything that comes later.

---

## Verification Before Module 05

Before moving on, you should be able to explain all of these without reopening the files:

1. Why debug assertions are not recoverable error handling.
2. Why `GameButtonState` stores both `ended_down` and `half_transitions`.
3. Why the runtime uses two `GameInput` snapshots instead of mutating one forever.
4. Why X11 timing tracks `work_seconds` and `total_seconds` separately.
5. Why letterboxing is presentation placement rather than backbuffer mutation.
6. Why fixed, dynamic-match, and dynamic-aspect modes are policy differences.

If those six answers feel stable, you are ready for the audio module.

---

## Bridge To Module 05

Module 04 taught a general pattern you should now recognize clearly:

```text
shared contract first
then per-frame runtime behavior
then backend-specific edge handling
```

Module 05 reuses that same pattern for sound.

Instead of a backbuffer contract plus display path, you will get a sample-buffer contract plus audio-device presentation path.
