# Lesson 72: World Context, HUD Context, and Zoom-Stable Panels

## Frontmatter

- Module: `14-runtime-integration-capstone`
- Lesson: `72`
- Status: Required
- Target files:
  - `src/platform.h`
  - `src/utils/render.h`
  - `src/game/main.c`
- Target symbols:
  - `platform_game_props_init`
  - `GameWorldConfig`
  - `make_render_context`
  - `game_make_world_context`
  - `game_make_hud_context`
  - `game_app_render`
  - `arena_begin_temp`
  - `arena_end_temp`

## Why This Lesson Exists

The runtime now carries two jobs at once:

- render scene-local world evidence
- render stable teaching panels on top of that world

If both jobs used the same camera transform, the diagnostic layer would drift, zoom, and become harder to read exactly when the learner needed it most.

This lesson explains why the final runtime always builds two render contexts per frame.

## Observable Outcome

By the end of this lesson, you should be able to explain the frame-composition contract: the runtime renders scene evidence through a camera-aware world context, renders teaching panels through a camera-neutral HUD context, and keeps render-time scratch allocations temporary inside one frame scope.

## First Reading Strategy

Read the composition path from foundation to frame:

1. default `GameWorldConfig` in `platform_game_props_init(...)`
2. `make_render_context(...)`
3. `game_make_world_context(...)`
4. `game_make_hud_context(...)`
5. `game_app_render(...)`

That order makes it easier to see that the HUD is not a separate render system. It is the same render system with a different camera policy.

## Step 0: Carry Forward the Control Lesson Properly

Lesson 71 established that the runtime can have missing scene state, override policy, and rebuild retries.

This lesson answers a different question: when a scene is live, how does the frame keep world evidence and diagnostic overlays readable at the same time? The answer is render-context separation, not duplicated drawing APIs.

## Step 1: Start at the Default World Configuration

`platform_game_props_init(...)` seeds `GameWorldConfig` with:

- left-bottom world origin
- `camera_zoom = 1.0f`
- `camera_x = 0.0f`
- `camera_y = 0.0f`

That is the neutral rendering contract for the whole platform.

Everything else is a deliberate override from scene or input state.

That neutral starting point matters. It means both world and HUD rendering begin from one shared platform contract before they intentionally diverge.

## Step 2: Read `make_render_context(...)` as the One-Time Math Bake

`src/utils/render.h` makes the main rendering rule explicit:

- camera and coordinate-origin math are baked once into `RenderContext`
- draw calls then use those baked coefficients without repeating branches

This is important for the capstone because it means world and HUD layers can each have a clean context of their own.

The runtime does not smear camera rules across every draw call.

It bakes them into the context boundary.

That is one of the cleanest architectural moves in the course. A complicated policy becomes one per-frame setup step instead of hundreds of repeated branches.

## Step 3: Read `game_make_world_context(...)` as Scene-Camera Adoption

`game_make_world_context(...)` copies the current `GameWorldConfig`, then overwrites camera fields from the active scene camera when one exists.

That means:

- world geometry follows scene pan and zoom
- scene-specific camera state stays local to the active level state

This is why the learner can pan around a scene without breaking the platform-wide HUD contract.

World movement remains scene-local because the camera fields are adopted here, not spread throughout unrelated HUD code.

## Step 4: Read `game_make_hud_context(...)` as a Deliberate Camera Reset

`game_make_hud_context(...)` takes the same base config and then forces:

- `camera_x = 0.0f`
- `camera_y = 0.0f`
- `camera_zoom = 1.0f`

That is the whole trick.

The HUD uses the same coordinate system machinery, but without the world camera.

So the labels, trace boxes, and warning panels remain readable even while the world surface moves.

That camera reset is not optional polish. It is the reason the runtime can be both interactive and teachable.

## Step 5: Read `game_app_render(...)` as a Three-Layer Frame

The final render order is simple and strong:

1. draw a background wash into the full backbuffer
2. call `game_render_scene(...)` with `world_ctx`
3. call `game_draw_hud(...)` with `hud_ctx`

That order tells you exactly what kind of runtime this is.

The scene produces the world proof.

The HUD interprets and annotates it.

That sequencing is the whole capstone composition model in one sentence.

## Step 6: Read Frame Scratch as Another Render Boundary

`game_app_render(...)` also begins a scratch `TempMemory` scope for the frame and ends it before returning.

That means temporary render strings and panel buffers belong to the frame, not to the scene lifetime.

The Arena lab even opens extra temporary scopes during rendering to make lifetime depth visible.

This is a good capstone detail because it shows that render composition and memory discipline are still coupled.

Even in the final integration chapter, the course keeps lifetime rules visible. Render-time strings, temp arrays, and panel text are frame-local, not scene-local.

## Step 7: Explain Why One Context Would Be Worse

If the runtime drew HUD panels with the world context:

- text would zoom with the camera
- panel layout would drift under pan
- proof surfaces would become hardest to read exactly when the learner manipulates the scene

The two-context design prevents that failure mode cleanly.

It also preserves backend simplicity: both backends still call the same `game_app_render(...)` facade and let the game layer manage the two-context policy internally.

## Worked Example: One Camera Move, Two Different Outcomes

Suppose you zoom the active scene camera in strongly.

What should happen?

1. world rectangles and labels move and scale through `world_ctx`
2. the trace panel, warning meter, and HUD text stay anchored and readable through `hud_ctx`
3. both still render into the same backbuffer and final display pass

That is the visible proof that the runtime chose context separation instead of one all-purpose camera transform.

## Common Mistake: Thinking `hud_ctx` Is a Separate UI System

It is not.

It is the same render-context system with a different camera policy.

That is what makes the design elegant.

## JS-to-C Bridge

This is like rendering a zoomable world layer and a fixed overlay layer with the same drawing primitives, but with two different transforms passed in at the top.

## Try Now

1. Open `src/platform.h` and explain why `camera_zoom` must default to `1.0f` rather than zero.
2. Open `src/utils/render.h` and explain what information is baked into `RenderContext` once per frame.
3. Open `src/game/main.c` and compare `game_make_world_context(...)` with `game_make_hud_context(...)`.
4. Explain why `game_app_render(...)` draws the scene before the HUD.

## Exercises

1. Explain why the HUD stays readable during camera pan and zoom.
2. Explain why the render layer still needs frame-scoped scratch memory even though scene data lives in the level arena.
3. Explain why the platform reuses one render-context abstraction instead of inventing a separate HUD rendering API.
4. Describe the full frame composition order from background wash to HUD panels.

## Runtime Proof Checkpoint

At this point, you should be able to explain the runtime's composition contract: world evidence is rendered through the active scene camera, diagnostic panels are rendered through a camera-neutral HUD context, and frame-scoped scratch allocations are released cleanly at the end of the render pass.

## Recap

This lesson explained the final frame composition model:

- platform defaults seed a neutral world config
- `make_render_context(...)` bakes per-frame transform math once
- `world_ctx` adopts the scene camera
- `hud_ctx` resets pan and zoom for stable overlays
- frame scratch scopes keep render-time allocations temporary

Next, we stay in the HUD and explain how logs, trace panels, warning context, and scene detail lines combine into one runtime-wide diagnostic system.
