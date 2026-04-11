# Module 01: Course Foundations

Module 01 exists to remove the most common kind of early confusion in low-level codebases: not "this concept is too advanced," but "I do not know which files actually define the live runtime."

This module gives you the minimum architectural footing needed before the course starts talking about windows, frame loops, pixels, input, timing, audio, allocators, scene machines, and HUD proof systems.

If you come from web development, think of this module as the phase where you answer these questions before trying to debug anything:

```text
what is the real entry point?
what is framework glue?
what is shared contract?
what is the actual app surface?
```

Without those answers, every later lesson feels noisier than it really is.

---

## Why This Module Comes First

The active course runtime has four layers you must be able to name before studying behavior in detail:

```text
build-dev.sh
  -> chooses a backend shell

src/platforms/*/main.c
  -> owns OS-facing setup, input polling, presentation, and audio device edges

src/platform.h
  -> defines the shared platform/game contract

src/game/main.h + src/game/main.c
  -> define the shared runtime facade and runtime behavior
```

This module teaches those four layers in that order.

---

## Observable Outcome

By the end of Module 01, you should be able to do all of the following from memory:

1. Draw the live runtime spine from `build-dev.sh` through one backend shell into the shared game facade.
2. Explain which build flags change the executable and which only change workflow.
3. Explain why `src/platform.h` exists and what resource ownership it centralizes.

That is the minimum architecture vocabulary needed before Module 02 starts booting real windows.

---

## Module Map

```text
Lesson 01
  course map, source parity, live runtime spine

Lesson 02
  build identity, backend selection, dev vs bench personality

Lesson 03
  shared contract, ownership bundle, startup helpers, cleanup symmetry
```

---

## Lesson Order

1. [lesson-01-course-map-and-source-parity.md](lesson-01-course-map-and-source-parity.md)
   - learn which files are the real front door and which are support or subsystem files
2. [lesson-02-build-script-backends-and-modes.md](lesson-02-build-script-backends-and-modes.md)
   - learn how the build script shapes the final executable
3. [lesson-03-platform-contract-skeleton-and-shared-types.md](lesson-03-platform-contract-skeleton-and-shared-types.md)
   - learn how both backend shells stay aligned through one shared contract

---

## How To Work Through This Module

Use the module as an architecture drill, not just as reading material.

For each lesson:

1. Read the observable outcome first.
2. Open the target file immediately.
3. Redraw the lesson's main diagram without looking after your first pass.
4. Answer the practice questions in writing, even if they seem easy.

That last step matters. Beginner intuition improves faster when you force yourself to state the architecture clearly instead of only recognizing it while reading.

---

## Core Habits To Build Here

This module is also teaching three habits that the rest of the course depends on.

### Habit 1: Separate live path from nearby historical/support material

The course tree contains active files plus historical or teaching-support material. Module 01 teaches you to anchor on the live runtime path instead of assuming every interesting-looking file is equally central.

### Habit 2: Separate shared contract from backend detail

The course has two backend shells, but one runtime. This only stays readable if you constantly ask:

```text
is this platform-specific detail?
or shared runtime policy?
```

### Habit 3: Read startup and cleanup together

The contract and backend layers are easier to trust when you pair resource acquisition with resource release. That habit starts here and becomes essential later in the allocator and audio modules.

---

## Beginner Danger Zones

Watch for these mistakes while reading Module 01:

1. Treating every public header like a top-level API.
2. Assuming a file is the front door just because it is compiled.
3. Assuming backend code owns game logic just because backend `main.c` calls into it.
4. Reading `build-dev.sh` like tooling trivia instead of as part of runtime architecture.

Module 01 is designed to prevent those mistakes before they spread into later lessons.

---

## Verification Before Module 02

Do not move on until you can explain all of these without opening the files again:

1. Why `course/` is the real root for lesson file paths.
2. Why `src/game/main.h` is the live runtime front door.
3. Why `build-dev.sh` matters to runtime behavior rather than only to tooling.
4. Why `PlatformGameProps` exists as one shared ownership bundle.

If you can explain those four points cleanly, you are ready to watch the architecture become real in a backend boot path.

---

## Bridge To Module 02

Module 01 gave you names, boundaries, and ownership vocabulary.

Module 02 turns that vocabulary into real platform boot code:

```text
create a window
claim shared runtime resources
bridge CPU memory to a presentation texture
start the game facade
run the frame loop
clean everything up in reverse order
```

That is the moment where the architecture stops being a diagram and becomes a running program.
