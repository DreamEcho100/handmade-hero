# Lesson 15: Frame Timing, the 60 FPS Budget, and Backend Pacing Strategy

## Frontmatter

- Module: `04-input-timing-and-presentation`
- Lesson: `15`
- Status: Required
- Target files:
  - `src/platform.h`
  - `src/platforms/raylib/main.c`
  - `src/platforms/x11/main.c`
- Target symbols:
  - `TARGET_FPS`
  - `timespec_to_seconds`
  - `timespec_diff_seconds`
  - `timing_begin`
  - `timing_mark_work_done`
  - `timing_sleep_until_target`
  - `timing_end`
  - `GetFrameTime`
  - `GetFPS`

## Observable Outcome

By the end of this lesson, you should be able to explain one frame budget numerically and describe how Raylib and X11 each try to stay near that target.

You should be able to explain why these are different concepts:

- target frame duration
- work time
- total frame time
- sleep time
- displayed FPS estimate

## Prerequisite Bridge

Lesson 14 gave the runtime a stable input snapshot per frame.

The next unavoidable question is:

```text
how long is one frame supposed to take,
how do we measure that honestly,
and how do the backends pace toward that target?
```

That is what this lesson makes explicit.

## Why This Lesson Exists

Input, rendering, and audio all live inside time.

So once the runtime can assemble a trustworthy frame snapshot, it needs a trustworthy time story too.

This lesson is not mostly about optimization.

It is about accounting:

- when did the frame begin?
- how long did active work take?
- how long did the whole frame take including waiting?
- who is responsible for pacing: the backend or an external runtime?

Without that accounting, later performance and audio lessons become vague.

## Step 1: Start With the Budget Constant

In `src/platform.h`:

```c
#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif
```

That implies a nominal target duration of:

$$
\frac{1}{60} \approx 0.01667 \text{ seconds} \approx 16.67 \text{ ms}
$$

That number is worth memorizing.

It is the runtime's default frame budget.

## Worked Example: One Budget Story

Suppose one frame spends:

- `4 ms` on input and update
- `2 ms` on audio processing
- `5 ms` on rendering and presentation preparation

Then:

```text
work time = 11 ms
```

Against a `16.67 ms` target, the remaining slack is about:

```text
16.67 - 11.00 = 5.67 ms
```

That leftover time is what a pacing system can sleep away if it wants to land near the 60 FPS target.

## Visual: One Frame Budget

```text
16.67 ms target
|--------------------------------|

work work work work work
                wait
```

If work exceeds the budget, the frame misses target.

If work stays below the budget, the runtime may sleep or let another system absorb the delay.

## Step 2: Raylib and X11 Use Different Timing Ownership Models

The course deliberately shows two timing styles.

### Raylib path

Raylib delegates much of the pacing to its own runtime:

```c
SetTargetFPS(TARGET_FPS);
...
game_app_update(game, curr_input, GetFrameTime(), &props.level);
...
game_app_render(game, &props.backbuffer, GetFPS(), props.scale_mode,
                props.world_config, ...);
```

This means:

- `SetTargetFPS(...)` tells Raylib the desired cadence
- `GetFrameTime()` provides the frame delta used by update logic
- `GetFPS()` provides a diagnostic estimate for the HUD or debug render path

Raylib owns most of the sleep policy internally.

### X11 path

X11 exposes the timing machinery explicitly:

- `timing_begin()` captures frame start
- `timing_mark_work_done()` records how long active work took
- `timing_sleep_until_target()` performs explicit waiting when vsync is unavailable
- `timing_end()` captures total frame duration

So X11 is the clearer backend for studying timing mechanics from first principles.

## Visual: Same Goal, Different Owner

```text
Raylib:
  configure target FPS once
  -> Raylib runtime handles coarse pacing
  -> game reads GetFrameTime() and GetFPS()

X11:
  capture times explicitly
  -> do the work
  -> maybe sleep explicitly
  -> compute final totals explicitly
```

Both aim at the same cadence.
The difference is who owns the clockwork.

## Step 3: Read the X11 Time Helpers as Unit Conversion First

In `src/platforms/x11/main.c`:

```c
static inline double timespec_to_seconds(const struct timespec *ts) {
  return (double)ts->tv_sec + (double)ts->tv_nsec * 1e-9;
}

static inline float timespec_diff_seconds(const struct timespec *a,
                                          const struct timespec *b) {
  return (float)(timespec_to_seconds(b) - timespec_to_seconds(a));
}
```

These helpers are small, but they are doing an important cleanup job.

Instead of forcing the rest of the backend to reason in separate seconds and nanoseconds fields, they convert timing math into one unit:

```text
seconds as a floating-point quantity
```

That makes later frame-accounting code much easier to read.

## Step 4: Understand the `FrameTiming` Struct as a Timeline Recorder

X11 stores:

```c
typedef struct {
  struct timespec frame_start;
  struct timespec work_end;
  struct timespec frame_end;
  float work_seconds;
  float total_seconds;
  float sleep_seconds;
} FrameTiming;
```

This is not “the timer.”

It is a record of three checkpoints and three derived values.

### Checkpoints

- `frame_start`
- `work_end`
- `frame_end`

### Derived numbers

- `work_seconds`
- `total_seconds`
- `sleep_seconds`

## Visual: The X11 Frame Timeline

```text
frame_start
   |
   +-- input
   +-- update
   +-- audio
   +-- render
   |
work_end
   |
   +-- optional wait
   |
frame_end
```

This diagram is the best way to keep the whole timing story straight.

## Step 5: Read the X11 Timing Helpers as Checkpoint Transitions

### Begin the frame

```c
static inline void timing_begin(void) { get_timespec(&g_timing.frame_start); }
```

This marks the start of all accounting for the frame.

### Mark work completion

```c
static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &g_timing.work_end);
}
```

This says:

```text
measure how long active work took before any optional waiting
```

### End the frame

```c
static inline void timing_end(void) {
  get_timespec(&g_timing.frame_end);
  g_timing.total_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &g_timing.frame_end);
  g_timing.sleep_seconds = g_timing.total_seconds - g_timing.work_seconds;
}
```

This computes the final totals after waiting has finished.

## Step 6: Keep `work_seconds` and `total_seconds` Separate

This distinction is one of the most important in the module.

Suppose a frame did `11 ms` of work and then waited `5 ms`.

Then:

```text
work_seconds  ≈ 11 ms
sleep_seconds ≈ 5 ms
total_seconds ≈ 16 ms
```

If you stored only the total frame time, you would not know whether the runtime was overloaded or comfortably under budget.

That is why X11 keeps those quantities distinct.

## Step 7: Understand the Two-Phase Sleep Strategy

The X11 sleep helper does not just sleep once blindly.

It uses two phases:

1. coarse `nanosleep` in 1 ms chunks while still far from the deadline
2. short spin-wait near the deadline

From the live code:

```c
float threshold = TARGET_SECONDS - 0.003f; /* 3ms spin budget */

while (elapsed < threshold) {
  struct timespec sleep_ts = {.tv_sec = 0, .tv_nsec = 1000000L};
  nanosleep(&sleep_ts, NULL);
  ...
}

while (elapsed < TARGET_SECONDS) {
  ... spin until precise target ...
}
```

### Why two phases?

Because one large sleep can overshoot the deadline badly.

So the helper does:

```text
sleep coarsely while far away
spin briefly only near the deadline
```

That balances CPU use against timing precision.

## Step 8: VSync Changes Who Pays for Waiting

In X11, the main loop ends with:

```c
timing_mark_work_done();
if (!g_x11.vsync_enabled) {
  timing_sleep_until_target();
}
timing_end();
```

That means:

- if vsync is active, the swap path may already pace the frame
- if vsync is not active, the backend uses the software frame limiter instead

This is important because it explains why “sleep policy” is conditional instead of always-on.

## Step 9: `dt` and FPS Are Related but Not the Same Thing

In Raylib:

- `GetFrameTime()` feeds simulation update
- `GetFPS()` feeds diagnostics or HUD output

In X11:

- `dt` comes from `g_timing.total_seconds` or a bootstrap fallback of `1.0f / TARGET_FPS`
- displayed FPS is computed from rolling timing data

Those are not interchangeable concepts.

### `dt`

Used by simulation to answer:

```text
how much virtual time passed before this update step?
```

### FPS

Used by diagnostics to answer:

```text
how many frames per second are we roughly achieving?
```

## Practical Exercises

### Exercise 1: Compute the Target Budget

What is the per-frame budget for `TARGET_FPS = 60`?

Expected answer:

```text
1 / 60 ≈ 0.01667 seconds ≈ 16.67 ms
```

### Exercise 2: Separate Work From Total

If a frame does `9 ms` of work and then waits `6 ms`, what are the three key numbers?

Expected answer:

```text
work  = 9 ms
sleep = 6 ms
total = 15 ms
```

### Exercise 3: Explain the Ownership Difference

In one sentence each, describe who owns pacing in Raylib and in X11.

Target answers:

```text
Raylib: the Raylib runtime owns most pacing after SetTargetFPS.
X11: the backend measures and performs pacing explicitly.
```

## Common Mistakes

### Mistake 1: Treating `dt`, frame budget, and FPS as the same quantity

They are related, but they answer different questions.

### Mistake 2: Thinking total frame time tells you everything you need

Without separating work time from sleep time, you lose useful performance information.

### Mistake 3: Thinking X11's manual timer code means it is a different runtime architecture

It is the same frame loop architecture with more explicit timing ownership.

## Troubleshooting Your Understanding

### “Why does the lesson care about `work_seconds` separately from `total_seconds`?”

Because “how long the frame lasted” and “how busy the runtime was” are not the same question.

### “Why use spin-wait at all?”

Because schedulers are imprecise near deadlines, and the final few milliseconds are where pacing accuracy matters most.

## Recap

You now have the frame-timing model for the course:

1. `TARGET_FPS` implies a real per-frame time budget
2. Raylib delegates much of pacing to its own runtime
3. X11 measures and paces frames explicitly
4. work time, total time, and sleep time are different quantities
5. `dt` drives simulation while FPS is primarily diagnostic

## Next Lesson

The next lesson moves from time back to presentation math.

Lesson 16 explains how to fit an existing canvas into a resized window without distorting it, using shared letterbox and aspect-fit helpers.
