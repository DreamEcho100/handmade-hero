# Lesson 38: Scene Requests, Wrapping, and Override Policy

## Frontmatter

- Module: `08-game-facade-and-scene-machine`
- Lesson: `38`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_wrap_scene`
  - `game_scene_navigation_delta`
  - `game_handle_scene_requests`
  - `current`
  - `requested`
  - `compile_time_override_active`
  - `compile_time_locked`
  - `runtime_override_active`
  - `runtime_override_locked`
  - `runtime_forced_scene`

## Observable Outcome

By the end of this lesson, you should be able to trace one scene-change request from raw button edges to the final authoritative `requested` value and explain why normal navigation is allowed to be replaced by stronger override policy.

## Prerequisite Bridge

Lesson 37 separated root app state, scene-machine policy, and current scene payload.

The next question is pure control flow:

```text
how does the runtime decide which scene it wants next?
```

This lesson answers that before any rebuild happens.

## Why This Lesson Exists

Multi-scene runtimes usually fail in one of two ways:

- scene switching is too implicit and hard to reason about
- override rules are scattered and silently win without being obvious

This branch avoids both by funnelling scene choice through a very small set of helpers.

## Step 1: Keep Three Layers Separate

Read the request path as three layers.

### Layer 1: input intent

What did the learner press this frame?

### Layer 2: candidate scene

What scene would normal navigation choose?

### Layer 3: authoritative request

After override policy is applied, what scene does the runtime actually want next?

If you keep those three layers separate, the code becomes much easier to trust.

## Step 2: Read `game_wrap_scene(...)`

The helper is:

```c
static GameSceneID game_wrap_scene(int value) {
  while (value < 0)
    value += GAME_SCENE_COUNT;
  return (GameSceneID)(value % GAME_SCENE_COUNT);
}
```

This is the runtime's circular-scene helper.

It guarantees that navigation stays inside the valid enum range.

## Worked Example: Negative Wrapping

Suppose `GAME_SCENE_COUNT == 4` and the current scene is `0`.

If the learner presses previous, the candidate value is:

```text
0 + (-1) = -1
```

The helper adds `GAME_SCENE_COUNT` until the value is non-negative:

```text
-1 + 4 = 3
```

Then the modulo keeps it in range.

So previous-from-zero becomes scene `3`, not an invalid ID.

## Visual: Circular Navigation

```text
0 -> 1 -> 2 -> 3 -> 0
^                   |
|___________________|
```

This is the navigation policy the helper enforces.

## Step 3: Read `game_scene_navigation_delta(...)`

The helper is:

```c
static int game_scene_navigation_delta(const GameInput *input) {
  if (!input)
    return 0;
  if (button_just_pressed(input->buttons.prev_scene))
    return -1;
  if (button_just_pressed(input->buttons.next_scene))
    return 1;
  return 0;
}
```

This function does one small job well:

```text
convert raw input edges into scene-navigation intent
```

It does not choose a final scene. It only emits `-1`, `+1`, or `0`.

That is good decomposition.

## Step 4: Why Delta Comes Before Candidate Scene

Returning an `int` delta instead of a `GameSceneID` directly is a good design choice because the helper stays policy-neutral.

It does not need to know:

- the current scene
- wrapping rules
- compile-time overrides
- runtime lock state

It only answers:

```text
did the learner ask to go left, right, or nowhere?
```

## Step 5: Read `game_handle_scene_requests(...)` as the Policy Funnel

This is the central request-policy function.

Its jobs are:

1. toggle the HUD if requested
2. toggle runtime override mode if requested
3. clear runtime override if requested
4. read navigation delta
5. compute candidate scene with wrapping
6. assign normal `requested`
7. reapply compile-time lock rules
8. reapply runtime lock rules

That order is the real logic of the scene-machine front end.

## Step 6: Read the Runtime Override Toggle Carefully

When runtime override is enabled, the code does:

```c
game->scene.runtime_override_active = 1;
game->scene.runtime_override_locked = 1;
game->scene.runtime_forced_scene = game->scene.current;
```

That means enabling runtime override does two things immediately.

### First

The override lane becomes active.

### Second

The forced scene is seeded from the current scene.

So runtime override does not begin in an undefined state. It begins by locking onto the scene the learner is already viewing.

## Step 7: Follow the Normal Navigation Path

When `nav_delta != 0`, the code does:

```c
candidate = game_wrap_scene((int)game->scene.current + nav_delta);
```

Then:

```c
if (game->scene.runtime_override_active) {
  game->scene.runtime_forced_scene = candidate;
  game->scene.requested = candidate;
} else {
  game->scene.requested = candidate;
}
```

That means normal navigation behaves differently depending on whether runtime override is active.

### Normal mode

Navigation updates `requested`.

### Runtime override mode

Navigation updates both `runtime_forced_scene` and `requested`.

That is a subtle but important difference.

## Step 8: Separate `current`, `requested`, and `runtime_forced_scene`

These three fields are easy to blur together if you read too fast.

### `current`

The scene whose payload is active right now.

### `requested`

The scene the control policy wants next.

### `runtime_forced_scene`

The scene runtime override mode wants to hold onto when that override is active and locked.

That is why the runtime needs all three. They represent different stages of control state.

## Worked Example: One `]` Press With No Override

Assume:

- `current = 1`
- runtime override is off
- learner presses `]`

Then:

1. `game_scene_navigation_delta(...)` returns `+1`
2. `game_wrap_scene(1 + 1)` returns `2`
3. `requested` becomes scene `2`
4. `current` remains `1` until the update loop later promotes `requested`

That last point matters. Request policy and actual scene switch are separate phases.

## Step 9: The Final Override Rules Are Explicit Reassignments

At the bottom of the function:

```c
if (game->scene.compile_time_override_active &&
    game->scene.compile_time_locked) {
  game->scene.requested = game->scene.compile_time_scene;
}
if (game->scene.runtime_override_active &&
    game->scene.runtime_override_locked) {
  game->scene.requested = game->scene.runtime_forced_scene;
}
```

This is the most important policy block in the lesson.

The code does not merely "remember" override state. It reapplies override state as the final assignment to `requested`.

That makes priority literal.

## Step 10: Which Override Wins on the Current Branch?

In the current code, the compile-time locked assignment runs first and the runtime locked assignment runs second.

So if both are active and both are locked, the second one wins because it writes last:

```text
requested = runtime_forced_scene
```

That is not theory. It is the exact policy encoded by this branch's assignment order.

## Visual: Full Request Funnel

```text
button edge
  -> navigation delta
  -> wrapped candidate scene
  -> requested scene
  -> compile-time lock may replace it
  -> runtime lock may replace it
  -> final requested scene
```

This is the control path you should remember.

## Practical Exercises

### Exercise 1: Explain `requested`

Why does the runtime need `requested` separate from `current`?

Expected answer:

```text
because policy chooses what scene should happen next before the runtime actually performs the transition and rebuild to make it current
```

### Exercise 2: Explain the Delta Helper

Why is it useful that `game_scene_navigation_delta(...)` returns `-1`, `+1`, or `0` instead of a scene ID directly?

Expected answer:

```text
because it keeps raw input intent separate from wrapping and override policy so later code can choose the final scene cleanly
```

### Exercise 3: Explain Override Priority

If both compile-time and runtime locked overrides are active, which one wins on this branch and why?

Expected answer:

```text
runtime override wins because its final assignment to requested happens after the compile-time assignment
```

## Common Mistakes

### Mistake 1: Treating overrides as just another input source

They are stronger policy layers, not merely more button presses.

### Mistake 2: Thinking navigation instantly changes the active scene

It only changes `requested`; the real switch happens later in `game_app_update(...)`.

### Mistake 3: Forgetting that runtime override stores its own forced target

That state is what lets the runtime keep enforcing its override lane.

## Troubleshooting Your Understanding

### "Why not write directly to `current` inside request handling?"

Because rebuild, counters, and side effects belong to the later transition phase, not to raw input parsing.

### "Why is override logic at the end instead of before navigation?"

Because the function wants to gather ordinary intent first, then let stronger policy rewrite that intent if necessary.

## Recap

You now understand the request-policy pipeline:

1. button edges become a navigation delta
2. the delta becomes a wrapped candidate scene
3. normal request logic assigns `requested`
4. compile-time and runtime override rules may replace that request
5. `current` still does not change until the later rebuild decision point

## Next Lesson

Lesson 39 follows that later decision point: when `requested` becomes different from `current`, when same-scene reload is treated differently, and how the level arena is reset and repopulated during scene rebuild.
