# Lesson 29: Camera Ownership, World vs HUD Contexts, and Visibility Culling

## Frontmatter

- Module: `06-coordinate-systems-and-camera`
- Lesson: `29`
- Status: Required
- Target files:
  - `src/game/base.h`
  - `src/utils/render_explicit.h`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
- Target symbols:
  - `GameCamera`
  - `game_default_camera`
  - `game_level_camera`
  - `camera_pan`
  - `screen_move`
  - `game_apply_camera_input`
  - `game_make_world_context`
  - `game_make_hud_context`
  - `world_rect_is_visible`

## Observable Outcome

By the end of this lesson, you should be able to explain the full chain from camera input to world-context bake to visibility testing, and also explain why the HUD stays stable while the world moves.

## Prerequisite Bridge

The previous lessons established:

- world units
- coordinate policy
- baked render contexts
- explicit helper boundaries
- HUD cursor layout

The remaining practical questions are:

```text
who owns the camera?
how does input move it?
why are there separate world and HUD contexts?
how do we skip drawing things that are fully off-screen?
```

Lesson 29 closes the coordinate module by answering all four.

## Why This Lesson Exists

Once camera motion exists, two new architecture pressures appear immediately:

1. the world should move relative to the camera
2. the HUD should not drift with the world

And once scenes can contain many objects, a third pressure appears:

3. completely off-screen geometry should be cheap to ignore

This lesson connects those three ideas into one coherent render pipeline.

## Step 1: Read `GameCamera` as Minimal View State

In `src/game/base.h`:

```c
typedef struct {
  float x;
  float y;
  float zoom;
} GameCamera;
```

This is intentionally small.

It stores only what this runtime currently needs:

- horizontal pan
- vertical pan
- zoom

That minimalism is useful. It keeps the camera lesson focused on view-state meaning rather than general engine features.

## Step 2: Notice the Camera Is Explicitly Initialized

In `src/game/main.c`:

```c
static GameCamera game_default_camera(void) {
  GameCamera camera;
  camera.x = 0.0f;
  camera.y = 0.0f;
  camera.zoom = 1.0f;
  return camera;
}
```

This matches an earlier rule from the course:

```text
zoom must start at 1.0, not 0.0
```

So camera state is not left to accidental defaults.

## Step 3: Read `game_level_camera(...)` as Scene-Owned Camera Selection

The live runtime does not keep one global camera for all scenes.

Instead, `game_level_camera(...)` switches on the active scene and returns the address of that scene's own camera field.

That means the ownership model is:

```text
each scene owns its own persistent camera state
the runtime selects the active scene camera at update/render time
```

This fits the live scene-lab architecture well because each scene can preserve its own pan and zoom independently.

## Step 4: Walk `game_apply_camera_input(...)`

The live input handler does four distinct jobs:

1. early-out if camera or input is missing
2. reset the camera on `cam_reset`
3. compute pan intent from held directional buttons
4. apply zoom in/out and clamp the zoom range

From the code:

```c
if (button_just_pressed(input->buttons.cam_reset))
  *camera = game_default_camera();

cam_dx = (input->buttons.cam_right.ended_down ? 3.0f : 0.0f) +
         (input->buttons.cam_left.ended_down ? -3.0f : 0.0f);
cam_dy = (input->buttons.cam_up.ended_down ? 3.0f : 0.0f) +
         (input->buttons.cam_down.ended_down ? -3.0f : 0.0f);
camera_pan(&camera->x, &camera->y, cam_dx, cam_dy, 1.0f, dt);
```

Then zoom is updated multiplicatively and clamped between `0.1f` and `20.0f`.

This is a clean small control loop.

## Worked Example: Why Reset Uses an Edge, Not a Held State

`cam_reset` uses `button_just_pressed(...)` instead of `ended_down` because reset is a one-shot action.

If it used a held-state check, holding the reset key would repeatedly overwrite the camera every frame.

This is a direct payoff from Module 04's input model.

## Step 5: Understand `camera_pan(...)` as a Convention-Aware Movement Helper

In `src/utils/render_explicit.h`, `camera_pan(...)` accepts a `y_sign` argument so it can remain convention-aware without knowing the whole origin enum.

That means its contract is:

```text
move the camera so content shifts in screen-space directions
while respecting the active vertical sign convention
```

The current `main.c` caller passes `1.0f`, which is narrower than the helper's full generality, but the helper itself is designed for the broader configurable-coordinate story.

That distinction is worth noticing.

## Step 6: `screen_move(...)` Is the Sibling Idea for Objects

Also in `render_explicit.h`, `screen_move(...)` applies sign-aware motion to world positions.

It exists for the same reason as `camera_pan(...)`:

```text
input-driven screen-relative motion should still behave sensibly when the world convention changes
```

So this lesson is not only about the camera.

It is also preparing the general mental model for movement in configurable coordinate systems.

## Step 7: Read the World and HUD Context Builders as Two Different Policies

From `src/game/main.c`:

```c
world_ctx = game_make_world_context(backbuffer, world_config, camera);
hud_ctx = game_make_hud_context(backbuffer, world_config);
```

The important distinction is:

### World context

Includes the active scene camera.

### HUD context

Forces:

```c
camera_x = 0.0f;
camera_y = 0.0f;
camera_zoom = 1.0f;
```

So the HUD stays pinned in place.

## Worked Example: Why the HUD Must Not Reuse the World Context

Suppose the player pans the camera right and zooms in.

The world should shift and scale accordingly.

The HUD should not.

If the HUD reused the world context blindly, the overlay text and panels would drift with the scene and become unreadable.

That is exactly why the runtime builds two separate contexts.

## Step 8: Read `world_rect_is_visible(...)` as the Cheap Draw-Skip Test

In `src/utils/render_explicit.h`:

```c
static inline int world_rect_is_visible(const RenderContext *ctx,
                                        float wx, float wy,
                                        float ww, float wh) {
  float px = world_rect_px_x(ctx, wx, ww);
  float py = world_rect_px_y(ctx, wy, wh);
  float pw = world_w(ctx, ww);
  float ph = world_h(ctx, wh);
  return (px + pw > 0.0f) && (px < ctx->px_w) &&
         (py + ph > 0.0f) && (py < ctx->px_h);
}
```

This is the render-time culling rule.

It says:

```text
after converting the world rectangle into pixel bounds,
does any part still overlap the viewport?
```

If not, the runtime can skip the draw.

## Step 9: Why the Visibility Helper Reuses the Same Conversion Functions

This is one of the best design choices in the module.

The culling helper does not invent a separate coordinate system.

It uses the same `world_rect_px_x(...)`, `world_rect_px_y(...)`, `world_w(...)`, and `world_h(...)` helpers that drawing uses.

That means:

```text
the thing used to decide visibility
and the thing used to decide draw placement
share the same conversion logic
```

That keeps culling honest.

## Visual: The Full Coordinate Pipeline

```text
scene-owned camera state
  -> game_apply_camera_input
  -> game_make_world_context
  -> world_rect_is_visible
      -> if visible: convert and draw
      -> if not visible: skip

separate HUD path
  -> game_make_hud_context
  -> draw overlay in stable screen-relative space
```

That is the full closing picture for Module 06.

## Practical Exercises

### Exercise 1: Explain Ownership

Why does the runtime look up a camera through `game_level_camera(...)` instead of storing one global camera for every scene?

Expected answer:

```text
because each scene owns persistent camera state and the runtime selects the active scene's camera rather than forcing one shared view for all scenes
```

### Exercise 2: Explain the Two Contexts

Why are both `game_make_world_context(...)` and `game_make_hud_context(...)` needed?

Expected answer:

```text
because world content should respond to camera pan/zoom while HUD content should remain pinned and readable on screen
```

### Exercise 3: Explain Visibility Culling

Why is `world_rect_is_visible(...)` reliable across different coordinate origins and camera states?

Expected answer:

```text
because it reuses the same world-to-pixel conversion helpers and the same baked RenderContext fields as the draw path itself
```

## Common Mistakes

### Mistake 1: Thinking camera state and render context are the same thing

The camera is persistent view state; the render context is one frame's baked conversion data.

### Mistake 2: Letting the HUD use the world context blindly

That makes overlay elements drift and zoom with the scene.

### Mistake 3: Treating culling as unrelated to conversion math

Good culling must use the same conversion logic as drawing or it becomes untrustworthy.

## Troubleshooting Your Understanding

### “Why does the course care about culling already?”

Because once camera motion exists, off-screen objects become normal, and the runtime needs a principled way to skip them.

### “Why is `camera_pan(...)` more general than the current call site?”

Because the helper is designed for the configurable-origin model even if the current game-side caller is using a narrower subset of that capability right now.

## Recap

You now have the full coordinate-and-camera pipeline:

1. each scene owns persistent camera state
2. input updates that state through small movement helpers
3. world rendering bakes the active camera into a world render context
4. HUD rendering uses a separate camera-stripped context
5. visibility culling reuses the same conversion helpers as drawing

## Module Bridge

Module 06 has now done the core coordinate work:

- world units and config meaning
- origin and sign policy
- baked render contexts
- explicit conversion helpers
- HUD flow
- camera ownership and culling

That brings the course to the next unavoidable systems question:

```text
when scenes rebuild,
what state survives,
what state dies,
and how should memory policy make that obvious?
```

Module 07 answers that through arenas, temp checkpoints, pools, and slabs.
