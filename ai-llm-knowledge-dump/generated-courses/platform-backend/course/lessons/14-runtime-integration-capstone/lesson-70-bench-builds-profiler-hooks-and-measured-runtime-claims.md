# Lesson 70: Bench Builds, Profiler Hooks, and Measured Runtime Claims

## Frontmatter

- Module: `14-runtime-integration-capstone`
- Lesson: `70`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/utils/perf.h`
  - `src/utils/perf.c`
  - `src/platforms/raylib/main.c`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
- Target symbols:
  - `--bench`
  - `--stress`
  - `ENABLE_PERF`
  - `BENCH_DURATION_S`
  - `PERF_BEGIN`
  - `PERF_BEGIN_NAMED`
  - `PERF_END`
  - `perf_print_all`
  - `game_app_render`

## Why This Lesson Exists

The scene modules taught you how to read allocator, render, and audio evidence inside one running frame.

This lesson explains how the runtime measures itself without changing its everyday development build.

That distinction matters.

The course does not treat performance as a permanent debug overlay.

It treats performance as a separate build personality with explicit entry and exit rules.

## Observable Outcome

By the end of this lesson, you should be able to explain the runtime's measurement contract: normal dev builds prioritize correctness and sanitizers, bench builds activate named profiling scopes and timed auto-exit, and measured claims should always be attached to a specific build personality.

## First Reading Strategy

Read the measurement path from outside in:

1. `build-dev.sh` and its `--bench` / `--stress` flags
2. `perf.h` and `perf.c` as the instrumentation boundary
3. backend main-loop bench exit in Raylib
4. existing runtime timer scopes such as `game_app/render_total`

That order matters because profiler output only makes sense if the learner first understands which build personality produced it.

## Step 0: Bridge Forward From Lesson 69 Correctly

Lesson 69 ended by locking one scene and proving one allocator story in isolation.

This lesson widens that workflow. The next question is no longer only `can I prove this scene?` It is `how do I measure the integrated runtime honestly without distorting the normal development loop?`

That is why the capstone starts with build personality.

## Step 1: Read `--bench` as a Build Personality Switch

In `build-dev.sh`, `--bench=N` does three important things together:

- switches from `-O0` plus sanitizers to `-O2 -DNDEBUG`
- defines `ENABLE_PERF`
- defines `BENCH_DURATION_S=N`

That means a bench build is not just a dev build with one extra print.

It is a different runtime contract designed to answer timing questions honestly.

That distinction is worth guarding carefully. If the learner forgets that optimization, sanitizer policy, and timed exit all changed together, the later numbers are easy to over-trust.

## Step 2: Read `perf.h` as a Zero-Cost Boundary in Normal Dev Builds

`src/utils/perf.h` is structured so that profiling code disappears when `ENABLE_PERF` is not defined.

In perf builds:

- `PERF_BEGIN(...)` and `PERF_END(...)` record timings
- `PERF_BEGIN_NAMED(...)` stores readable timer names
- `perf_print_all()` prints the summary table

In normal dev builds:

- all timing macros collapse to no-ops
- `perf_print_all()` becomes an empty inline function

This is the correct design for a teaching runtime.

Instrumentation exists when you ask for it and vanishes when you do not.

That zero-cost boundary is one of the healthiest engineering choices in the course. It lets the runtime keep clean production-style profiling hooks without paying a permanent cost in ordinary development builds.

## Step 3: Read the Backend Main Loop as the Owner of Bench Exit

The bench build does not rely on the learner to quit manually.

In `src/platforms/raylib/main.c`, the loop checks:

```c
#ifdef BENCH_DURATION_S
if (GetTime() >= (double)BENCH_DURATION_S) {
  printf("[bench] ...");
  perf_print_all();
  break;
}
#endif
```

That is the important integration point.

The platform loop, not the scene logic, owns the end of the timed run.

This keeps measurement policy outside scene code.

That is exactly the right ownership boundary. Scene code should not decide how long the benchmark runs or when a platform session ends.

## Step 4: Read the Timer Names as Evidence of Real Runtime Boundaries

The current runtime already marks meaningful sections such as:

- `raylib/process_audio`
- `raylib/demo_render+display`
- `game_app/render_total`

Those names matter because they tell you what is being measured.

The capstone standard is not only to collect numbers.

It is to collect numbers whose scope is obvious.

Named scopes are what make later reasoning possible. If the summary only printed anonymous durations, the learner would know that time was spent but not which runtime boundary owned it.

## Step 5: Place `--stress` Correctly

`build-dev.sh` still accepts `--stress=N`, but its teaching role is narrower than `--bench`.

The stress hook is tied to the historical explicit render demo in `src/game/demo_explicit.c`, where `STRESS_RECTS` adds many culled world rectangles behind `world_rect_is_visible(...)`.

That means:

- `--bench` is part of the live four-scene runtime workflow
- `--stress` is a supporting measurement artifact for the older explicit render path

The distinction is useful.

It prevents the learner from claiming that every old micro-benchmark is still the main course runtime.

That continuity point matters here because the capstone is about the live facade path. Historical benchmarks still matter, but they need to be labeled honestly.

## Step 6: Run One Short Bench and Read the Result Conservatively

Use:

```bash
./build-dev.sh --backend=raylib --bench=5 -r
```

Then read the profiler summary as a comparison tool, not a trophy.

The correct question is:

```text
which named runtime region dominates this measured build personality?
```

The wrong question is:

```text
did I get a fast-looking number once?
```

The correct posture is comparative and scoped. Bench mode tells you which named region dominated this run under this build personality on this backend. That is a useful engineering claim. `It felt fast once` is not.

## Worked Example: Reading One Bench Result Conservatively

Suppose a 5-second Raylib bench ends and the top row is `game_app/render_total`.

What can you say safely?

1. under bench settings, overall render work dominated total recorded time
2. the runtime correctly reached the backend-owned auto-exit boundary
3. named scopes were active and summary printing worked
4. you still need comparison runs before claiming a regression or improvement

That is a disciplined measurement habit, and that is what this lesson is trying to teach.

## Common Mistake: Treating Perf Macros as Always-On Debug State

They are not.

They are deliberately compiled away outside bench mode.

That keeps the normal development loop focused on correctness, sanitizers, and visible proof surfaces.

Bench mode is a deliberate temporary personality, not the new default life of the program.

## JS-to-C Bridge

This is closer to a production build with explicit `performance.mark(...)` instrumentation than to leaving `console.time(...)` calls scattered through all code paths forever.

## Try Now

1. Run `./build-dev.sh --backend=raylib --bench=5 -r` and confirm the program exits by itself after printing a profiler summary.
2. Open `src/utils/perf.h` and explain why the macros become zero-cost when `ENABLE_PERF` is absent.
3. Open `src/platforms/raylib/main.c` and explain why the bench stop condition lives in the platform loop instead of scene code.
4. Open `src/game/demo_explicit.c` and explain why `--stress` should be described as a supporting benchmark artifact, not the main four-scene runtime path.

## Exercises

1. Explain why `--bench` changes optimization and sanitizer behavior at the same time.
2. Explain why named timers are better teaching tools than anonymous elapsed-time prints.
3. Explain why the backend loop, not `game_app_render(...)`, decides when the bench run ends.
4. Describe the difference between the live bench workflow and the older stress-rect benchmark hook.

## Runtime Proof Checkpoint

At this point, you should be able to explain the runtime's measurement contract: normal dev builds prioritize correctness, bench builds activate explicit profiler scopes and timed auto-exit, and the older `--stress` hook remains a useful but narrower render benchmark artifact.

## Recap

This lesson established the final runtime measurement model:

- `--bench` creates a real measurement build personality
- `perf.h` disappears cleanly outside bench mode
- the platform main loop owns timed shutdown and summary printing
- named timers make runtime cost claims inspectable
- `--stress` still exists, but as a historical supporting benchmark path rather than the core four-scene runtime

Next, we connect runtime control to diagnostics by tracing how scene requests, override locks, and rebuild failures interact inside the live scene machine.
