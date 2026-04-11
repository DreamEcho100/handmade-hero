# Lesson 40: Scene-Enter Side Effects, Camera Ownership, and Audio Identity

## Frontmatter

- Module: `08-game-facade-and-scene-machine`
- Lesson: `40`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/game/audio_demo.c`
- Target symbols:
  - `game_default_camera`
  - `game_level_camera`
  - `game_apply_camera_input`
  - `game_audio_clear_voices`
  - `game_audio_apply_scene_profile`
  - `game_audio_trigger_scene_enter`
  - `scene_enter_count`
  - `frame_entered`

## Observable Outcome

By the end of this lesson, you should be able to explain why scene enter is a coordinated cross-system event rather than a plain enum swap, and how camera ownership, scene timestamps, logs, audio profile changes, and entry cues all participate in that event chain.

## Prerequisite Bridge

Lesson 39 explained when scene rebuild happens and how the level arena is reset.

That still leaves one more question:

```text
once the new scene payload exists,
what else has to change so the runtime really feels like it entered a new scene?
```

This lesson answers that.

## Why This Lesson Exists

If scene transitions only changed an ID and rebuilt memory, they would still be incomplete.

The runtime also needs to:

- give the new scene a sane camera baseline
- select the right scene-owned camera for later input and rendering
- update transition timestamps and counters
- reconfigure persistent audio identity
- emit an immediate entry cue

That collection of side effects is what makes "scene enter" a real event.

## Step 1: Start With the Camera Baseline Helper

The default camera helper is:

```c
static GameCamera game_default_camera(void) {
  GameCamera camera;
  camera.x = 0.0f;
  camera.y = 0.0f;
  camera.zoom = 1.0f;
  return camera;
}
```

This is the scene-enter camera baseline.

It encodes three initial truths:

- no pan
- no pan
- neutral zoom

The important part is not the numbers alone.

It is the policy:

```text
new scenes start from known camera state unless that scene later changes it
```

## Step 2: Notice That Camera Ownership Belongs to Scene Payloads

Each scene fill function initializes its own camera field with `game_default_camera()`:

- `game_fill_arena_lab(...)`
- `game_fill_level_reload(...)`
- `game_fill_pool_lab(...)`
- `game_fill_slab_audio(...)`

That means camera ownership is scene-local, not global.

This is the correct design because each scene payload lives in the level arena and should own its own view state.

## Step 3: Read `game_level_camera(...)` as the Scene-Owned Camera Selector

The selector is:

```c
static GameCamera *game_level_camera(GameLevelState *level_state) {
  if (!level_state)
    return NULL;
  switch (level_state->scene_id) {
  case GAME_SCENE_ARENA_LAB:
    return &level_state->as.arena_lab.camera;
  case GAME_SCENE_LEVEL_RELOAD:
    return &level_state->as.level_reload.camera;
  case GAME_SCENE_POOL_LAB:
    return &level_state->as.pool_lab.camera;
  case GAME_SCENE_SLAB_AUDIO_LAB:
    return &level_state->as.slab_audio_lab.camera;
  default:
    return NULL;
  }
}
```

This helper answers one important question centrally:

```text
which camera belongs to the active scene payload right now?
```

That avoids sprinkling scene-specific camera branching through unrelated code.

## Step 4: Read `game_apply_camera_input(...)` as Shared Policy Over Scene-Owned State

The helper:

- resets the camera on `cam_reset`
- computes pan intent from directional buttons
- calls `camera_pan(...)`
- applies multiplicative zoom in/out
- clamps zoom to `[0.1, 20.0]`

The architectural point is not the camera math itself.

It is that one shared input policy operates on whichever `GameCamera` the active scene owns.

So the design is:

```text
scene payload owns the camera object
shared logic updates that camera object
```

That is a good scene-machine pattern.

## Worked Example: Why Reset Lives in Shared Camera Input

The reset rule:

```c
if (button_just_pressed(input->buttons.cam_reset))
  *camera = game_default_camera();
```

means every scene gets a consistent one-shot way to restore its camera baseline.

The baseline is scene-agnostic, but the camera object being reset is still scene-owned.

That is exactly the right split.

## Step 5: Read the Transition Timestamp and Counter Updates

In the transition path of `game_app_update(...)`, the runtime does:

```c
game->scene.previous = game->scene.current;
game->scene.current = game->scene.requested;
game->scene.frame_entered = game->frame_index;
game->transition_count++;
game_rebuild_scene(game, level, 1);
```

This tells you that scene enter includes at least four pieces of state change before the rebuild even runs:

- remember the old scene
- promote the new scene
- timestamp when it was entered
- increment transition count

That is already more than an enum swap.

## Step 6: Read the Audio Side Effects at the End of `game_rebuild_scene(...)`

After the new scene payload is filled, the function does:

```c
game_log_eventf(game, "%s %s", entering_new_scene ? "enter" : "reload",
                game_scene_name(current));
game_audio_apply_scene_profile(&game->audio, (int)current);
game_audio_trigger_scene_enter(&game->audio, (int)current);
```

These lines are the strongest proof that scene enter is a cross-system handoff.

### `game_log_eventf(...)`

Records a visible lifecycle event for the HUD log.

### `game_audio_apply_scene_profile(...)`

Reconfigures persistent audio identity.

### `game_audio_trigger_scene_enter(...)`

Adds an immediate audible cue on top of that profile change.

## Step 7: Read `game_audio_apply_scene_profile(...)` as Long-Lived Audio Identity Switching

In `src/game/audio_demo.c`, the profile helper does much more than choose a melody.

It changes:

- pattern set
- pattern count
- music volume
- SFX volume
- tone volume
- step timing

And it also resets important runtime state such as:

- one-shot voices via `game_audio_clear_voices(...)`
- sequencer position
- tone state

So scene enter is not only visual. It rebuilds the audible personality of the runtime.

## Step 8: Read `game_audio_trigger_scene_enter(...)` as the Immediate Cue Layer

The entry cue helper gives each scene a direct short feedback sound.

Examples on this branch:

- scene 0 triggers a low tone
- scene 1 triggers a mid tone
- scene 2 triggers a high tone
- scene 3 layers high and low for a thicker cue

That means the audio system models two different scene-enter responsibilities.

### Persistent profile

What the scene should sound like over time.

### Immediate cue

What the learner should hear right now as confirmation that scene enter happened.

## Worked Example: Why Profile and Entry Cue Must Be Separate

If `game_audio_apply_scene_profile(...)` also tried to serve as the entry cue, the runtime would mix up long-lived identity with one-shot transition feedback.

Those are different jobs.

The current branch keeps them separate, which makes the design easier to reason about.

## Visual: Full Scene-Enter Chain

```text
scene request accepted
  -> previous/current/frame_entered/transition_count updated
  -> level arena rebuilt
  -> new scene payload created
  -> scene-owned camera initialized to default baseline
  -> log entry recorded
  -> audio profile switched
  -> entry cue triggered
  -> later update/render use the new scene's camera and payload
```

That is the actual scene-enter event chain.

## Step 9: Why Camera and Audio Side Effects Live in Different Places

This is a subtle architectural point.

### Camera baseline

Lives in scene fill functions because the camera object is scene-owned payload.

### Audio profile switch

Lives in the shared rebuild path because audio is a persistent cross-scene system reacting to scene identity.

Both are part of scene enter, but they belong at different structural layers.

That separation is correct and worth noticing.

## Practical Exercises

### Exercise 1: Explain Camera Ownership

Why does each scene payload store its own `GameCamera` instead of the scene machine storing one global camera?

Expected answer:

```text
because camera state is part of the active scene payload and should rebuild or reset with that scene's level-lifetime data rather than living as unrelated global control state
```

### Exercise 2: Explain `frame_entered`

Why does the runtime record `frame_entered` when a new scene becomes current?

Expected answer:

```text
so the runtime can remember exactly when the current scene was entered for transition evidence, HUD display, and lifecycle tracking
```

### Exercise 3: Explain Audio Side Effects

Why does scene enter need both `game_audio_apply_scene_profile(...)` and `game_audio_trigger_scene_enter(...)`?

Expected answer:

```text
because one sets the long-lived scene audio identity while the other provides an immediate one-shot cue confirming that the transition happened
```

## Common Mistakes

### Mistake 1: Treating scene enter as only a memory event

It is also a camera, logging, timestamp, and audio event.

### Mistake 2: Thinking the scene machine owns the camera object

It owns scene-selection policy, not scene-local camera payload.

### Mistake 3: Confusing audio profile switching with the scene-enter cue

One is persistent identity; the other is immediate feedback.

## Troubleshooting Your Understanding

### "Why not reset the camera from backend code?"

Because the camera belongs to scene-owned game payload, not to the platform shell.

### "Why do log entries belong in the scene-enter path?"

Because the runtime wants visible evidence that transitions and reloads actually happened, not only an unseen internal state change.

## Recap

You now understand why scene enter is a coordinated event:

1. transition metadata updates before rebuild
2. scene-owned camera state is initialized inside the new payload
3. shared logic later selects and updates the active scene's camera
4. the rebuild path logs the lifecycle event
5. audio profile and entry cues react to the new scene identity

## Module Bridge

Module 08 has now established the scene machine end to end:

- tiny public facade
- internal ownership tree
- request and override policy
- grouped scene rebuild
- coordinated enter side effects

That sets up Module 09's main question:

```text
once the runtime can switch scenes cleanly,
how does it prove to the learner what each scene has actually demonstrated?
```
