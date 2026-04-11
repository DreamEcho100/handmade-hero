# Lesson 71: Rebuild Failures, Retry Paths, and Override Locks

## Frontmatter

- Module: `14-runtime-integration-capstone`
- Lesson: `71`
- Status: Required
- Target files:
  - `build-dev.sh`
  - `src/game/main.c`
- Target symbols:
  - `game_handle_scene_requests`
  - `game_rebuild_scene`
  - `game_app_update`
  - `--force-scene`
  - `--lock-scene`
  - `COURSE_FORCE_SCENE_INDEX`
  - `COURSE_LOCK_SCENE`
  - `runtime_override_active`
  - `runtime_override_locked`
  - `compile_time_override_active`
  - `compile_time_locked`
  - `rebuild_failure_active`
  - `rebuild_failure_scene`

## Why This Lesson Exists

By lesson 69, the course had shown how to isolate one scene and prove one allocator story.

This lesson explains how the runtime keeps control stable when scene selection and scene rebuilding do not line up perfectly.

That is the capstone control problem.

The runtime must answer three questions every frame:

- which scene does the learner want
- which scene is currently authoritative
- what happens if rebuilding that scene fails right now

## Observable Outcome

By the end of this lesson, you should be able to explain the runtime control contract: scene requests express intent, override layers can clamp that intent, rebuilds happen centrally, and rebuild failure becomes a visible retry state instead of a hidden crash or corrupted transition.

## First Reading Strategy

Read the control path in time order:

1. `game_handle_scene_requests(...)`
2. compile-time and runtime override policy
3. `game_rebuild_scene(...)`
4. the missing-`level_state` retry path in `game_app_update(...)`
5. the HUD lines that explain those states back to the learner

That order mirrors the runtime and keeps request intent separate from authoritative scene state.

## Step 0: Keep Measurement and Control Separate

Lesson 70 taught you how the runtime changes build personality for measurement.

This lesson returns to control policy. The important bridge is that bench builds and dev builds still rely on the same scene machine. Measurement does not replace control correctness.

## Step 1: Read Scene Requests as Intent, Not Immediate State

`game_handle_scene_requests(...)` updates `game->scene.requested`.

It does not directly rebuild the scene.

That separation matters.

Navigation input records intent first.

Then `game_app_update(...)` decides whether it needs to:

- build the first scene
- transition into a different scene
- reload the current scene

This is cleaner than mixing request parsing and rebuild work in one place.

It also gives the runtime a place to tell the truth about uncertainty. A request can exist before the next scene is actually live.

## Step 2: Read Compile-Time Overrides as the Strongest Lock

`build-dev.sh` can define:

- `COURSE_FORCE_SCENE_INDEX`
- `COURSE_LOCK_SCENE`

Those become compile-time override state inside the scene machine.

In `game_handle_scene_requests(...)`, if compile-time override is active and locked, the request is forced back to the compile-time scene.

That means the learner can still press navigation keys, but the runtime will not honor those requests.

The lock is explicit.

That explicitness matters pedagogically. The runtime does not merely ignore navigation input mysteriously. It applies a policy layer the HUD can later explain.

## Step 3: Read Runtime Override as a Second, Softer Lock

The same function also supports runtime override controls:

- toggle runtime override on and off
- lock it to the current scene
- clear it explicitly

This is different from compile-time locking.

Runtime override is interactive.

The learner can enter the running program, freeze the current scene, and later clear that decision without rebuilding the executable.

That makes runtime override a great debugging tool. It is softer than compile-time forcing because it belongs to the current process, not to the build artifact.

## Step 4: Read Rebuild Failure as a Recoverable State

`game_rebuild_scene(...)` begins by clearing `game->level_state`, resetting the level arena, and trying to allocate a fresh `GameLevelState`.

If that allocation fails, it does not crash the loop.

Instead it records:

- `rebuild_failure_active = 1`
- `rebuild_failure_scene = current`
- `rebuild_failure_frame = frame_index`
- incremented `rebuild_failure_count`

Then it logs the failure and returns.

That makes rebuild failure visible, countable, and retryable.

This is one of the capstone's strongest engineering patterns. Failure is promoted to public runtime state instead of being buried in one failed allocation site.

## Step 5: Read `game_app_update(...)` as the Retry Owner

The next frame, `game_app_update(...)` checks:

```c
if (!game->level_state) {
  game_rebuild_scene(game, level, 1);
}
```

That means rebuild failure does not create a dead runtime.

It creates a runtime waiting for scene reconstruction to succeed.

While that state is active:

- audio warning decay still runs
- audio update still runs
- render can still show HUD messaging about the missing scene state

That means the learner can still observe the system honestly even when the scene payload is absent. The runtime narrows what it claims rather than pretending the scene exists.

## Visual: Failure-To-Retry Timeline

```text
request or reload requires rebuild
  -> game_rebuild_scene(...) attempts fresh level state
  -> allocation or rebuild step fails
  -> rebuild_failure_active becomes true
  -> level_state stays NULL
next frame:
  -> game_app_update(...) sees missing level_state
  -> game_rebuild_scene(...) is tried again
  -> HUD continues reporting the retry state until rebuild succeeds
```

That timeline is the core capstone control story: failure does not disappear, and it does not silently corrupt the scene machine. It becomes explicit retry state.

## Step 6: Read the HUD as the Public Explanation of Control State

The HUD prints both override status and rebuild failure status.

You can see:

- compile-time override on or off
- runtime override on or off
- whether either override is locked
- which scene is currently forced
- whether rebuild retry is pending

That is exactly the right capstone behavior.

The control system does not hide its own policy.

It prints it.

That is what turns this into a teachable runtime instead of a black box.

## Step 7: Use One Forced Build and One Runtime Lock to Prove Priority Order

Use a locked compile-time build first:

```bash
./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r
```

Then compare that with a normal run where you toggle the runtime override from inside the program.

The learner should be able to explain this priority order:

1. compile-time locked override wins when present
2. otherwise runtime locked override wins when active
3. otherwise navigation requests can change the requested scene

## Worked Example: One Requested Scene That Never Becomes Live

Suppose input requests a new scene while the level arena is too small to rebuild it successfully.

What should the learner conclude?

1. `requested` changed first
2. the runtime attempted a rebuild
3. authoritative scene state did not become live because `level_state` stayed null
4. failure metadata and HUD warning lines now describe the retry condition

That is a stronger mental model than simply saying `scene switching broke`.

## Common Mistake: Treating `requested` as If the Scene Already Changed

That is not the model.

`requested` is intent.

The authoritative scene changes only when `game_app_update(...)` transitions and `game_rebuild_scene(...)` successfully produces a new `level_state`.

That distinction is one of the main continuity bridges into Lessons 74 and 75, where scene isolation and full-course verification depend on understanding when a scene is actually live.

## JS-to-C Bridge

This is like separating router intent from mounted page state. A route change request is not the same thing as the new page already being constructed successfully.

## Try Now

1. Run `./build-dev.sh --backend=raylib --force-scene=2 --lock-scene -r` and confirm scene navigation does not escape the forced Pool lab.
2. Open `src/game/main.c` and trace how `game_handle_scene_requests(...)` updates `requested` before any rebuild occurs.
3. Explain how `game_rebuild_scene(...)` records failure without crashing the runtime.
4. Explain why `game_app_update(...)` is the place that retries a missing `level_state`.

## Exercises

1. Explain the difference between compile-time override locking and runtime override locking.
2. Explain why rebuild failure state stores both the scene id and the frame index.
3. Explain why the runtime continues updating audio even when `level_state` is unavailable.
4. Describe the full priority order from input request to authoritative scene selection.

## Runtime Proof Checkpoint

At this point, you should be able to explain the runtime control contract: requests are recorded first, overrides can clamp those requests, scene rebuilds happen centrally, and failures become visible retry states instead of silent corruption or hard crashes.

## Recap

This lesson connected control flow to diagnostics:

- scene requests are separate from scene rebuilds
- compile-time and runtime overrides are explicit policy layers
- rebuild failure is recorded as runtime state, not hidden
- `game_app_update(...)` owns retrying missing scene state
- the HUD exposes the active control policy to the learner

Next, we shift from control to composition and explain how the runtime keeps the world camera and the diagnostic HUD in separate render contexts inside one frame.
