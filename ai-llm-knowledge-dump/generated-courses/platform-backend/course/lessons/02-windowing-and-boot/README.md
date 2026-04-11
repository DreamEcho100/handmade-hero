# Module 02: Windowing and Boot

Module 02 is where the architecture from Module 01 first becomes real.

Up to this point, the course has mostly answered naming and ownership questions:

```text
what is the live runtime path?
what does the build script shape?
what is the shared contract?
```

This module changes the question to:

```text
how does the program claim a real window,
attach shared runtime resources to it,
and enter a stable frame loop?
```

That makes Module 02 the first true platform-facing module in the course.

---

## Why This Module Exists

Beginners often make one of two opposite mistakes when they first open backend code.

Mistake one:

```text
the backend must be the whole engine
```

Mistake two:

```text
the backend is just glue, so I can ignore it
```

Both are wrong.

The backend shell is not the whole runtime, but it is where the runtime gains real OS resources:

- a window
- a graphics context or presentation texture
- input events
- audio device edges
- startup failure handling
- teardown ordering

Module 02 teaches you to respect that layer without letting it swallow the rest of the architecture.

---

## Observable Outcome

By the end of Module 02, you should be able to:

1. Trace the Raylib boot path from `InitWindow(...)` to the main loop.
2. Trace the X11 and GLX boot path without confusing X11 resources with GL resources.
3. Explain why both backends still rely on the same shared startup and cleanup helpers.
4. Explain the startup and teardown path as one ownership story instead of separate unrelated steps.

If you can do that, Module 03's pixel-pipeline lessons will feel grounded instead of abstract.

---

## Module Map

```text
Lesson 04
  Raylib boot path: smallest complete backend shell

Lesson 05
  X11 + GLX boot path: explicit display, window, context, and texture ownership

Lesson 06
  shared startup and shared cleanup: why both shells still form one runtime
```

---

## Lesson Order

1. [lesson-04-open-a-raylib-window.md](lesson-04-open-a-raylib-window.md)
   - use Raylib to learn the backend boot shape with less API noise
2. [lesson-05-open-an-x11-glx-window.md](lesson-05-open-an-x11-glx-window.md)
   - unfold the same shape into explicit X11 and GLX resource acquisition
3. [lesson-06-shared-startup-backbuffer-and-cleanup-path.md](lesson-06-shared-startup-backbuffer-and-cleanup-path.md)
   - reconnect both backend shells to the same ownership model

---

## The Architecture Claim of This Module

This whole module is really defending one sentence:

```text
the backend owns platform-facing handles,
but shared startup still owns the common runtime bundle
```

That sentence is the bridge between Module 01's boundaries and Module 03's pixel-memory model.

---

## How To Study This Module

Do not read the backend code as one uninterrupted wall of APIs.

Read it as a repeated sequence of stages:

```text
claim platform resources
claim shared runtime resources
connect shared pixels to a presentation path
initialize the game facade
enter the frame loop
clean up in reverse order
```

For each lesson:

1. Identify which resources are backend-only.
2. Identify which resources come from `PlatformGameProps`.
3. Check the failure path immediately after the success path.
4. Say out loud what must be destroyed if a later step fails.

That last step is important. In systems work, cleanup is not an appendix. It is part of the design.

---

## What To Watch For

Module 02 is also training three reading skills.

### Skill 1: Classify resources by owner

Examples:

- Raylib texture and audio stream belong to `RaylibState`
- X11 `Display`, `Window`, and `GLXContext` belong to `X11State`
- backbuffer, audio buffer, and arenas belong to `PlatformGameProps`

### Skill 2: Separate "platform became real" from "runtime became ready"

A window can exist before the shared backbuffer exists.

A shared backbuffer can exist before the game facade exists.

That ordering matters.

### Skill 3: Read startup and teardown as mirror images

Every successful acquisition creates a cleanup obligation. Module 02 keeps repeating that rule because it becomes essential later in audio, allocators, and scene reloads.

---

## Beginner Danger Zones

Watch for these mistakes while reading Module 02:

1. Treating `RaylibState` or `X11State` as if they replace `PlatformGameProps`.
2. Confusing X11 resources with GL resources in the explicit backend.
3. Looking only at the happy-path startup code and skipping failure unwind.
4. Forgetting that the shared game facade still sits behind both backends.

The module is designed to correct those instincts early.

---

## Verification Before Module 03

Before moving on, you should be able to explain all of these from memory:

1. Why `RaylibState` is not a replacement for `PlatformGameProps`.
2. Why `X11State` contains both X11 handles and GL-related state.
3. Why both backends call `platform_game_props_init(...)`.
4. Why cleanup must run in the reverse direction of startup.

If those answers feel stable, you are ready for the next question:

```text
what exactly is the CPU image both backends are presenting?
```

That question begins Module 03.
