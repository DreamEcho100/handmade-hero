# Recommended Resources for Game Dev / Systems Programming Journey

> Tailored for Mazen (DreamEcho100) — a web dev transitioning to C/systems/game dev,
> following Handmade Hero, who wants deep "no magic" explanations.
>
> Organized by **what gives the most bang for your time right now**, not just by topic.

---

## Tier 1: Watch These First (directly accelerate your Handmade Hero journey)

### C Beyond the Basics

- **Casey Muratori — "Clean Code, Horrible Performance"** — Casey directly addressing why OOP/"clean code" patterns destroy performance. As someone coming from JS/React, this will rewire your thinking. Transcript would make an excellent course on "why data-oriented design matters."

- **Casey Muratori — "The Only Unbreakable Law"** (talk at Molly Rocket) — On API design and interface decisions. Directly relevant to your platform abstraction layer.

- **Mike Acton — "Data-Oriented Design and C++" (CppCon 2014)** — THE foundational talk on data-oriented design. This changed how an entire generation thinks about memory and performance. Acton was engine lead at Insomniac Games. Every single concept applies to your C work. **This is probably the single most important talk for where you are right now.**

- **Andrew Kelley — "A Practical Guide to Applying Data-Oriented Design"** — Zig creator, but the concepts are pure C-applicable. Shows concrete before/after refactors with performance numbers.

### Graphics Programming for Noobs

- **Bisqwit — "Creating a Doom-style 3D engine in C"** — Writes software renderers in C from scratch, explains every line. No libraries, no magic, just math and pixels. Transcript would be gold.

- **javidx9 (OneLoneCoder) — "Code-It-Yourself! 3D Graphics Engine" series** — You already follow javidx9 for your game ports. His 3D engine series goes from "what is a triangle" to perspective projection, clipping, and texturing. All in a console. Perfect stepping stone before OpenGL.

- **ssloy — "Tiny Renderer" (GitHub wiki, not YouTube)** — You already have the TinyRaytracer course in your repo. His Tiny Renderer teaches software rasterization step by step. The wiki itself reads like a transcript.

---

## Tier 2: High-Impact Talks (watch when you have an evening free)

### Game Engine Architecture

- **Jonathan Blow — "Preventing the Collapse of Civilization"** — On why software is getting worse over time and why understanding what's under the hood matters. Will reinforce your "refuse to move forward until fundamentals are solid" philosophy. Not technical per se, but deeply motivating for your trajectory.

- **Jonathan Blow — "A Programming Language for Games" (multiple talks)** — Blow designed Jai specifically to solve the problems you're encountering. Even if you never use Jai, his analysis of what's wrong with C/C++ for games is invaluable. Especially the talks on compile times, metaprogramming, and data-oriented design.

- **Ryan Fleury — "UI Series" (rfleury.com / substack)** — You already have his arena talk. His UI programming series on his substack is the best resource on building immediate-mode UI from scratch. Since he built the RAD Debugger's UI with arenas, this directly connects to the memory management course.

### Game Programming Patterns

- **Bob Nystrom — "Game Programming Patterns" (book, free online at gameprogrammingpatterns.com)** — Not a video, but this is THE resource. Covers: game loops, update methods, component systems, event queues, object pools, spatial partitioning, double buffering. You're already implementing many of these in your detour games. The book would name and systematize what you're discovering intuitively.

- **Brian Will — "Object-Oriented Programming is Bad" + "Entity Component Systems"** — Two videos that bridge the gap between your JS/OOP background and the data-oriented patterns used in game engines. Very direct, no-nonsense style.

### Game Development (practical)

- **Sebastian Lague — "Coding Adventures" series** — Raymarching, procedural planets, hydraulic erosion, boids, pathfinding. Builds each system from scratch with clear visual explanations. Not C (uses Unity/C#), but the algorithms and math transfer directly. His style of "let me show you the math, then build it" matches your learning preference.

- **The Cherno — "Game Engine" series** — Building a game engine from scratch in C++. More relevant once you're past Handmade Hero day ~100. The early episodes on project setup, logging, event systems, and layering are useful now though.

---

## Tier 3: For When You're Ready (after resuming Handmade Hero)

### Game Design

- **GDC Vault — "Juice It or Lose It" (Martin Jonasson & Petri Purho)** — The classic talk on making games FEEL good. Screen shake, particles, tweening, feedback. As someone building games from raw pixels, this will teach you what to add after the mechanics work. Short, fun, immediately applicable.

- **Mark Brown — "Game Maker's Toolkit" (YouTube channel)** — The best game design analysis channel. Episodes on level design, difficulty curves, movement mechanics. Watch selectively — episodes on games you've played.

### Storytelling / Narrative

- **Premature for now.** You're building mechanical intuition with arcade games. Narrative design matters when you're building games with story, much further down the road. If curious, watch **"The Art of Game Design" by Jesse Schell** (GDC talks), but don't prioritize this.

### UI/UX

- **Ryan Fleury's UI series** (mentioned above) covers the technical side. For game UI specifically, **Tes_3D — "Game UI Database"** is a visual reference site, not a video, but useful when you're designing menus/HUDs.

---

## Best Candidates for Transcript-to-Course Treatment

Given your pattern of turning transcripts into structured courses (like the Ryan Fleury arena talk → memory management course):

| Priority | Video/Talk                                                | Course It Would Become                                                   |
| -------- | --------------------------------------------------------- | ------------------------------------------------------------------------ |
| 1        | Mike Acton — "Data-Oriented Design and C++"               | Data-Oriented Design in C course                                         |
| 2        | Casey Muratori — "Clean Code, Horrible Performance"       | Performance-Aware Programming course                                     |
| 3        | Bisqwit — "Creating a Doom-style 3D engine in C"          | Software Rendering from Scratch course                                   |
| 4        | javidx9 — "Code-It-Yourself! 3D Graphics Engine"          | 3D Math and Projection course                                            |
| 5        | Jonathan Blow — "Preventing the Collapse of Civilization" | (motivational essay, not a code course — but the transcript is quotable) |

---

## Honest Take

**Don't spread too thin.** Your about-me reveals you already struggle with splitting focus between Handmade Hero, the detour games, the platform course, your web work, and other projects. Adding 15 new video courses to consume would make that worse.

**Actual recommendation:** Pick **ONE** from Tier 1 (the Mike Acton talk), watch it, then go back to your asteroids game and finish it. The best learning you can do right now is **finishing things**, not consuming more content. You paused at HH day 32 to build mechanical intuition — that was the right call. But the intuition comes from completing projects, not from watching more talks.

The talks will be there when you need them. Finish asteroids. Then frogger. Then resume Handmade Hero.
