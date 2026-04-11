# Lesson 12: Debug Trap, Assertions, and Fail-Fast Invariants

## Frontmatter

- Module: `04-input-timing-and-presentation`
- Lesson: `12`
- Status: Required
- Target files:
  - `src/game/base.h`
- Target symbols:
  - `DEBUG_TRAP`
  - `DEBUG_ASSERT`
  - `ASSERT_MSG`

## Observable Outcome

By the end of this lesson, you should be able to classify a failure correctly as one of three things:

- an internal invariant failure that should break immediately in debug builds
- a recoverable failure that should use normal control flow
- a user or environment problem that should not be treated as a logic invariant

That distinction matters because the rest of Module 04 introduces more persistent state, and bad state becomes more expensive every frame it survives.

## Prerequisite Bridge

Module 03 gave the runtime a visible image and on-screen debug text.

Module 04 starts by adding the complementary engineering rule:

```text
if the runtime reaches a state that should be impossible,
stop immediately instead of continuing to build more frames on bad assumptions
```

That is what the debug macros in `src/game/base.h` are for.

## Why This Lesson Exists

As the runtime grows, more facts must remain true across frames:

- the backbuffer pointer should stay valid
- camera zoom should stay sane
- the input buffers should carry coherent state
- later, arena lifetimes and audio state must remain valid too

If one of those internal truths breaks and the frame continues anyway, every later symptom becomes harder to trust.

So before the course teaches richer frame state, it teaches the rule for impossible state:

```text
fail early at the first trustworthy detection point
```

## Step 1: Read the Live Macro Block as Policy First

The core block in `src/game/base.h` is:

```c
#if defined(DEBUG)
#if defined(__i386__) || defined(__x86_64__)
#define DEBUG_TRAP() __asm__ volatile("int3")
#elif defined(__aarch64__)
#define DEBUG_TRAP() __asm__ volatile("brk #0")
#else
#include <signal.h>
#define DEBUG_TRAP() raise(SIGTRAP)
#endif
#define DEBUG_ASSERT(cond)                                                     \
  do {                                                                         \
    if (!(cond)) {                                                             \
      DEBUG_TRAP();                                                            \
    }                                                                          \
  } while (0)
#define ASSERT_MSG(cond, msg)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      (void)(msg);                                                             \
      DEBUG_TRAP();                                                            \
    }                                                                          \
  } while (0)
#else
#define DEBUG_TRAP() ((void)0)
#define DEBUG_ASSERT(cond) ((void)(cond))
#define ASSERT_MSG(cond, msg) ((void)(cond))
#endif
```

If you read it as raw preprocessor syntax, it looks busy.

If you read it as engineering policy, it says something simple:

```text
debug build  -> install real breakpoint behavior
release build -> remove trap behavior
```

That is the core of the lesson.

## Visual: The Decision Tree

```text
DEBUG defined?
  yes
    -> choose a real trap instruction or SIGTRAP fallback
    -> assertions can stop execution immediately
  no
    -> trap becomes no-op
    -> assertion expressions still compile safely
```

## Step 2: Separate the Three Macros by Job

### `DEBUG_TRAP()`

Use this when the code already knows it must stop immediately.

It has no condition.

It simply means:

```text
break here right now
```

### `DEBUG_ASSERT(cond)`

Use this when the code expects some internal statement to be true.

If it is false, the runtime traps.

### `ASSERT_MSG(cond, msg)`

This behaves like `DEBUG_ASSERT(cond)`, but also lets the code document the invariant in words.

The message is not a logging system.

It is primarily there so readers can see the intended truth next to the condition.

## Step 3: Why `DEBUG_TRAP()` Is Architecture-Aware

The file selects different trap mechanisms depending on the CPU family:

- x86 and x64 use `int3`
- AArch64 uses `brk #0`
- other targets fall back to `raise(SIGTRAP)`

That detail is worth noticing because it teaches a low-level habit:

```text
even debug-only behavior still has to respect the real machine underneath
```

This is not abstract “assertion theory.”
It is actual machine-aware fail-fast behavior.

## Step 4: Understand the Debug vs Release Split Precisely

In debug builds:

```text
assertions can stop the program the moment an invariant fails
```

In release builds:

```text
DEBUG_TRAP()      -> does nothing
DEBUG_ASSERT(x)   -> becomes ((void)(x))
ASSERT_MSG(x, m)  -> becomes ((void)(x))
```

That means these macros are not ordinary runtime error handling.

They are development-time truth checks.

## Step 5: Why `((void)(cond))` Exists in Release Builds

This line is subtle and important:

```c
#define DEBUG_ASSERT(cond) ((void)(cond))
```

It preserves two useful properties:

1. the expression still gets compiled and type-checked
2. if the expression has side effects, those side effects are not silently erased

The rule here is:

```text
release builds remove the trap,
not the responsibility to write sane assertion expressions
```

That is why you should still avoid putting surprising side effects inside assertions even though the macro preserves them.

## Step 6: What Counts as an Invariant

An invariant is a statement the runtime believes must already be true if the internal program state is healthy.

Examples that fit the model:

```c
DEBUG_ASSERT(backbuffer != NULL);
DEBUG_ASSERT(backbuffer->pixels != NULL);
ASSERT_MSG(world_config.camera_zoom > 0.0f,
           "camera zoom must remain positive");
```

These are not outside-world failures.

They are internal assumptions that later code depends on.

## Step 7: What Does Not Count as an Invariant

These cases should not usually use assertions:

- `XOpenDisplay(...)` failed
- audio initialization failed
- a file could not be opened
- the user closed the window

Those are external or recoverable conditions.

They need normal control flow, error returns, warnings, or graceful degradation.

## Worked Classification Table

| Situation                                                  | Correct tool        | Why                                |
| ---------------------------------------------------------- | ------------------- | ---------------------------------- |
| `XOpenDisplay(...)` returns `NULL`                         | normal failure path | external environment failure       |
| camera zoom becomes `0.0f` after startup guarantees `1.0f` | assertion           | internal invariant is broken       |
| audio device unavailable                                   | normal failure path | runtime may continue without audio |
| developer wants to stop at a known impossible branch       | `DEBUG_TRAP()`      | unconditional fail-fast checkpoint |

## Step 8: Why This Matters Before Input and Timing

The next lessons introduce state that spans frames:

- buttons with per-frame transition counts
- previous and current input snapshots
- frame timing checkpoints
- scale-mode policy and resize consequences

If that kind of state drifts into nonsense and the runtime keeps going, later symptoms become ambiguous.

Assertions give the codebase a disciplined way to say:

```text
this is not a recoverable path;
the runtime's internal story is already broken
```

## Worked Example: Good vs Bad Use

### Good use

```c
ASSERT_MSG(props->world_config.camera_zoom > 0.0f,
           "camera zoom must remain positive");
```

Why it is good:

- the camera zoom is part of internal game/platform state
- later math assumes it is valid
- continuing with `0.0f` would make coordinate mapping degenerate

### Bad use

```c
DEBUG_ASSERT(platform_audio_init(&props.audio_config) == 0);
```

Why it is bad:

- audio init can fail because of the user's machine or environment
- the runtime already has a real fallback path for “continue without audio”
- that is a normal runtime condition, not proof of corrupted internal logic

## Practical Exercises

### Exercise 1: Classify the Failure

For each case, choose `assert`, `normal failure path`, or `either but explain why`:

1. `backbuffer->pixels == NULL` right before drawing
2. `platform_audio_init(...)` fails on one Linux machine
3. `camera_zoom` becomes negative after only internal code touched it
4. the user presses the window close button

Expected answers:

```text
1 -> assert
2 -> normal failure path
3 -> assert
4 -> normal failure path
```

### Exercise 2: Explain the Release Behavior

Why is `DEBUG_ASSERT(cond)` not defined as plain `((void)0)` in release builds?

Expected answer:

```text
so the expression still compiles and its side effects are not silently removed
```

### Exercise 3: State the Rule in One Sentence

Write the lesson's central rule without looking back.

Target answer:

```text
assertions are for impossible internal states, not ordinary runtime failures
```

## Common Mistakes

### Mistake 1: Using assertions as general error handling

That collapses two very different kinds of failure into one tool.

### Mistake 2: Thinking `ASSERT_MSG` is useless because it does not print

The message still documents the invariant directly in code.

### Mistake 3: Thinking fail-fast logic belongs only in backend code

These macros live in shared game-layer code because invariants are shared program truths, not platform quirks.

## Troubleshooting Your Understanding

### “I still do not know when to use an assert”

Ask this question:

```text
if this condition is false,
does that prove my own internal program state is already wrong?
```

If yes, an assertion is probably appropriate.

If no, it is probably ordinary control flow.

### “Why not just print an error and continue?”

Because continuing after internal truth has failed often destroys the only trustworthy debugging context you had.

## Recap

You now have the fail-fast rule that the rest of Module 04 depends on:

1. debug traps stop immediately at impossible states
2. assertions check internal truths, not external failures
3. release builds remove trap behavior without erasing expression compilation
4. correct classification of failure types is part of sound systems engineering

## Next Lesson

The next lesson applies the same idea of “frame truth” to input.

Instead of asking “is a key down right now?”, Lesson 13 defines what a button report must mean for one whole frame.
